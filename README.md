# Projet 5 — Surveillance de la qualité de l'air intérieur

Mini-projet IoT — **ESP32 DevKit C v4 + Wokwi + Blynk**
Master GLCC Informatique — FS Ibn Tofail Kenitra

## 🎯 Description

Système connecté de surveillance de la qualité de l'air d'une pièce. Il mesure la
**température** et l'**humidité** (DHT22), un **indice CO2/COV simulé** (potentiomètre)
et une **température secondaire** (thermistor NTC). Un **indice de confort global**
pondéré est calculé, l'air est classé en 3 niveaux (**sain / modéré / pollué**),
affiché sur un **OLED SSD1306** (2 écrans alternés) et transmis à **Blynk**.

## 🔧 Composants

| Composant | Rôle | Broche ESP32 |
|---|---|---|
| DHT22 | Température + humidité | GPIO 4 |
| Potentiomètre | Indice CO2/COV simulé | GPIO 34 (ADC) |
| NTC thermistor | Température secondaire | GPIO 35 (ADC) |
| OLED SSD1306 I2C | Afficheur principal | SDA 21 / SCL 22 |
| LED verte | État « sain » | GPIO 25 |
| LED orange | État « modéré » | GPIO 26 |
| LED rouge | État « pollué » | GPIO 27 |
| Buzzer | Alerte critique | GPIO 14 |

## 🧮 Indice de confort global

```
Indice global = T(30%) + HR(30%) + CO2(40%)
```

| État | Indice | LED | Buzzer |
|---|---|---|---|
| Sain | ≥ 60 | Verte | Éteint |
| Modéré | 30–59 | Orange | Éteint |
| Pollué | < 30 | Rouge | Bip intermittent |

## 📡 Intégration Blynk

| Pin | Donnée |
|---|---|
| V0 | Température |
| V1 | Humidité |
| V2 | Indice CO2 (simulé) |
| V3 | Indice global (+ graphique d'historique) |
| V4 | LED couleur d'état (vert/orange/rouge) |

Événement `air_pollue` → notification push à l'entrée en état pollué, et 2ème
notification si l'air reste pollué plus de 30 minutes.

## ▶️ Lancer la simulation

1. Ouvrir le projet sur **Wokwi** : <https://wokwi.com/projects/465825450297283585>
2. Copier `sketch.ino`, `diagram.json` et la liste `libraries.txt` dans Wokwi.
3. Renseigner vos identifiants Blynk dans `sketch.ino` :
   ```cpp
   #define BLYNK_TEMPLATE_ID   "VOTRE_TEMPLATE_ID"
   #define BLYNK_TEMPLATE_NAME "VOTRE_TEMPLATE_NAME"
   #define BLYNK_AUTH_TOKEN    "VOTRE_AUTH_TOKEN"
   ```
4. Appuyer sur ▶️ **Play**.

> ⚠️ **Sécurité :** ne publiez jamais votre véritable `BLYNK_AUTH_TOKEN` dans un
> dépôt public. Les valeurs ci-dessus sont des espaces réservés.

## 📂 Contenu du dépôt

- `sketch.ino` — code Arduino C++ (structuré en fonctions, commenté, non bloquant)
- `diagram.json` — schéma de câblage Wokwi
- `libraries.txt` — bibliothèques Wokwi requises
- `wokwi.toml` — configuration Wokwi
- `Rapport_Projet5_Qualite_Air.docx` — rapport complet

## 👤 Auteur

Mohamed El Haimer — Master GLCC, FS Ibn Tofail Kenitra — 2025/2026
