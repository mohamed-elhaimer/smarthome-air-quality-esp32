/* =====================================================================
   PROJET 5 - Surveillance de qualite de l'air interieur
   ESP32 DevKit C v4 + Wokwi + Blynk (Wi-Fi simule Wokwi-GUEST)
   Capteurs : DHT22, Potentiometre (CO2 simule), NTC (T secondaire)
   Sorties  : OLED SSD1306, 3 LEDs (vert/orange/rouge), Buzzer
   ===================================================================== */

// ---- Blynk (doit etre AVANT les includes Blynk) ----
// IMPORTANT : remplacez par vos propres identifiants Blynk.
#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "YOUR_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_AUTH_TOKEN"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <math.h>

// ============================================================
// PIN DEFINITIONS (conformes au cahier de charge)
// ============================================================
#define PIN_DHT         4
#define PIN_POT         34
#define PIN_NTC         35
#define PIN_LED_GREEN   25   // air sain
#define PIN_LED_ORANGE  26   // qualite moderee
#define PIN_LED_RED     27   // air pollue
#define PIN_BUZZER      14   // alerte critique

#define DHT_TYPE DHT22
DHT dht(PIN_DHT, DHT_TYPE);

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---- Wi-Fi simule Wokwi ----
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// ---- NTC ----
const float NTC_BETA = 3950.0;   // coefficient Beta du thermistor Wokwi

// ---- Variables d'etat ----
float temperature  = 22.0;   // T DHT22 (principale)
float tempNTC      = 22.0;   // T NTC   (secondaire)
float humidity     = 50.0;
int   co2Index     = 0;      // 0-100 (potentiometre)
float globalIndex  = 0;      // indice global pondere
int   airQuality   = 0;      // 0=sain 1=modere 2=pollue
int   lastAirQuality = 0;

// ---- Timers (millis, jamais delay dans loop) ----
unsigned long lastSensorRead   = 0;
unsigned long lastScreenSwitch = 0;
unsigned long lastBuzzerToggle = 0;
unsigned long lastBlynkSend    = 0;
unsigned long pollueStartTime  = 0;   // instant d'entree en etat pollue
bool          pollueNotified30 = false;

int  currentScreen = 0;
int  lastScreen    = -1;
bool buzzerState   = false;

// ---- Compteur de temps par etat (Extension : temps/jour) ----
unsigned long stateSeconds[3] = {0, 0, 0};

/* -------------------------------------------------------------
   setup : initialise capteurs, OLED, broches et Wi-Fi/Blynk.
   La connexion Blynk est NON bloquante : la boucle tourne meme
   si le serveur Blynk est lent ou injoignable.
   params : aucun | retour : void
   ------------------------------------------------------------- */
void setup() {
  Serial.begin(115200);
  Serial.println("=== SETUP START ===");

  pinMode(PIN_LED_GREEN,  OUTPUT);
  pinMode(PIN_LED_ORANGE, OUTPUT);
  pinMode(PIN_LED_RED,    OUTPUT);
  pinMode(PIN_BUZZER,     OUTPUT);

  // Test rapide des sorties (delay autorise UNIQUEMENT dans setup)
  digitalWrite(PIN_LED_GREEN, HIGH);  delay(300); digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_ORANGE, HIGH); delay(300); digitalWrite(PIN_LED_ORANGE, LOW);
  digitalWrite(PIN_LED_RED, HIGH);    delay(300); digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_BUZZER, HIGH);     delay(200); digitalWrite(PIN_BUZZER, LOW);

  dht.begin();

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20); display.println("Air Quality Monitor");
  display.setCursor(30, 40); display.println("Demarrage...");
  display.display();

  // Connexion Wi-Fi + Blynk NON bloquante
  WiFi.begin(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect(5000);   // 5s max puis on continue quoi qu'il arrive

  Serial.println("=== SETUP DONE ===");
}

/* -------------------------------------------------------------
   readNTC : lit le thermistor NTC et convertit en degres C.
   params : aucun | retour : float (temperature en C)
   ------------------------------------------------------------- */
float readNTC() {
  int raw = analogRead(PIN_NTC);
  if (raw <= 0)    raw = 1;       // protection division
  if (raw >= 4095) raw = 4094;
  float celsius = 1.0 / (log(1.0 / (4095.0 / raw - 1.0)) / NTC_BETA
                  + 1.0 / 298.15) - 273.15;
  return celsius;
}

/* -------------------------------------------------------------
   readSensors : lit DHT22, NTC et potentiometre (CO2 simule).
   Gere les valeurs invalides (NaN) du DHT22.
   params : aucun | retour : void
   ------------------------------------------------------------- */
void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;     // garde derniere valeur valide
  if (!isnan(h)) humidity    = h;

  tempNTC = readNTC();

  int rawPot = analogRead(PIN_POT);
  co2Index = map(rawPot, 0, 4095, 0, 100);

  Serial.printf("T:%.1f T_NTC:%.1f H:%.1f CO2:%d Idx:%.0f Etat:%d\n",
                temperature, tempNTC, humidity, co2Index, globalIndex, airQuality);
}

/* -------------------------------------------------------------
   calculateIndex : calcule l'indice global pondere
   T(30%) + HR(30%) + CO2(40%) et determine l'etat (0/1/2).
   La temperature utilisee = moyenne DHT22 + NTC (capteur secondaire).
   params : aucun | retour : void
   ------------------------------------------------------------- */
void calculateIndex() {
  float tMoy = (temperature + tempNTC) / 2.0;   // utilisation du NTC

  float tScore;
  if (tMoy < 10 || tMoy > 35)       tScore = 0;
  else if (tMoy < 16 || tMoy > 28)  tScore = 40;
  else if (tMoy < 18 || tMoy > 24)  tScore = 70;
  else                              tScore = 100;

  float hScore;
  if (humidity < 20 || humidity > 80)      hScore = 0;
  else if (humidity < 30 || humidity > 70) hScore = 40;
  else if (humidity < 40 || humidity > 60) hScore = 70;
  else                                     hScore = 100;

  float co2Score = 100 - co2Index;

  globalIndex = (tScore * 0.30) + (hScore * 0.30) + (co2Score * 0.40);
  globalIndex = constrain(globalIndex, 0, 100);

  if (globalIndex >= 60)      airQuality = 0;   // sain
  else if (globalIndex >= 30) airQuality = 1;   // modere
  else                        airQuality = 2;   // pollue
}

/* -------------------------------------------------------------
   updateLEDs : allume la LED correspondant a l'etat courant.
   params : aucun | retour : void
   ------------------------------------------------------------- */
void updateLEDs() {
  digitalWrite(PIN_LED_GREEN,  airQuality == 0);
  digitalWrite(PIN_LED_ORANGE, airQuality == 1);
  digitalWrite(PIN_LED_RED,    airQuality == 2);
}

/* -------------------------------------------------------------
   updateBuzzer : fait sonner le buzzer (intermittent) seulement
   en etat critique (pollue) ; silencieux sinon.
   params : aucun | retour : void
   ------------------------------------------------------------- */
void updateBuzzer() {
  if (airQuality == 2) {
    if (millis() - lastBuzzerToggle >= 500) {
      buzzerState = !buzzerState;
      digitalWrite(PIN_BUZZER, buzzerState);
      lastBuzzerToggle = millis();
    }
  } else {
    digitalWrite(PIN_BUZZER, LOW);
    buzzerState = false;
  }
}

/* -------------------------------------------------------------
   getAdvice : retourne un conseil contextuel (Extension).
   params : aucun | retour : const char* (texte conseil)
   ------------------------------------------------------------- */
const char* getAdvice() {
  if (co2Index > 60) return "Ouvrez fenetre";
  if (humidity < 30) return "Humidificateur";
  if (humidity > 70) return "Trop humide";
  return "Air confortable";
}

/* -------------------------------------------------------------
   showScreen1 : ecran OLED des valeurs brutes
   ------------------------------------------------------------- */
void showScreen1() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);  display.println("=== Capteurs ===");
  display.setCursor(0, 12); display.printf("Temp : %.1f C", temperature);
  display.setCursor(0, 24); display.printf("T-NTC: %.1f C", tempNTC);
  display.setCursor(0, 36); display.printf("Humid: %.1f %%", humidity);
  display.setCursor(0, 48); display.printf("CO2:%d%%  Idx:%.0f", co2Index, globalIndex);
  display.display();
}

/* -------------------------------------------------------------
   showScreen2 : ecran OLED de l'etat global + conseil contextuel
   ------------------------------------------------------------- */
void showScreen2() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); display.println("=== Etat Air ===");
  display.setTextSize(2);
  display.setCursor(0, 18);
  if      (airQuality == 0) display.println("SAIN");
  else if (airQuality == 1) display.println("MODERE");
  else                      display.println("POLLUE");
  display.setTextSize(1);
  display.setCursor(0, 50); display.print("> "); display.println(getAdvice());
  display.display();
}

/* -------------------------------------------------------------
   updateDisplay : alterne les 2 ecrans OLED toutes les 3 secondes.
   params : aucun | retour : void
   ------------------------------------------------------------- */
void updateDisplay() {
  if (millis() - lastScreenSwitch >= 3000) {
    currentScreen = !currentScreen;
    lastScreenSwitch = millis();
    lastScreen = -1;
  }
  if (currentScreen != lastScreen) {
    if (currentScreen == 0) showScreen1();
    else                    showScreen2();
    lastScreen = currentScreen;
  }
}

/* -------------------------------------------------------------
   sendToBlynk : envoie les mesures vers Blynk (V0-V4).
   V0=Temp V1=Humid V2=CO2 V3=IndiceGlobal V4=LED couleur d'etat
   params : aucun | retour : void
   ------------------------------------------------------------- */
void sendToBlynk() {
  if (!Blynk.connected()) return;   // gestion Wi-Fi / serveur perdu

  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, co2Index);
  Blynk.virtualWrite(V3, globalIndex);

  const char* color = (airQuality == 0) ? "#23C48E"     // vert
                    : (airQuality == 1) ? "#ED9D00"     // orange
                                        : "#D3435C";    // rouge
  Blynk.setProperty(V4, "color", color);
  Blynk.virtualWrite(V4, 255);
}

/* -------------------------------------------------------------
   handleNotifications : alertes Blynk a l'entree en etat pollue,
   2eme alerte si pollue depuis plus de 30 minutes.
   params : aucun | retour : void
   ------------------------------------------------------------- */
void handleNotifications() {
  if (airQuality == 2 && lastAirQuality != 2) {
    pollueStartTime  = millis();
    pollueNotified30 = false;
    Blynk.logEvent("air_pollue", "Alerte : air interieur POLLUE !");
  }
  if (airQuality == 2 && !pollueNotified30 &&
      (millis() - pollueStartTime >= 30UL * 60UL * 1000UL)) {
    Blynk.logEvent("air_pollue", "Air pollue depuis plus de 30 minutes !");
    pollueNotified30 = true;
  }
  lastAirQuality = airQuality;
}

/* -------------------------------------------------------------
   loop : boucle principale non bloquante (millis, jamais delay).
   ------------------------------------------------------------- */
void loop() {
  Blynk.run();

  if (millis() - lastSensorRead >= 2000) {
    readSensors();
    calculateIndex();
    updateLEDs();
    handleNotifications();
    stateSeconds[airQuality] += 2;            // ~2s par cycle (Extension)
    lastSensorRead = millis();
  }

  if (millis() - lastBlynkSend >= 2000) {
    sendToBlynk();
    lastBlynkSend = millis();
  }

  updateBuzzer();
  updateDisplay();
}
