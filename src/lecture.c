
#include <Arduino.h>
#include <DHT.h>

// Broches du HC-SR04
#define TRIG_PIN 5
#define ECHO_PIN 18

// Broche du DHT11
#define DHT_PIN 15
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

// Mesure de la distance
void tacheUltrason(void *parameter) {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    while (true) {
        // Envoyer une impulsion au HC-SR04
        digitalWrite(TRIG_PIN, LOW);
        delayMicroseconds(2);

        digitalWrite(TRIG_PIN, HIGH);
        delayMicroseconds(10);

        digitalWrite(TRIG_PIN, LOW);

        // Mesurer la durée de l'écho
        unsigned long duree = pulseIn(ECHO_PIN, HIGH, 30000);

        if (duree == 0) {
            Serial.println("[HC-SR04] Aucune mesure");
        } else {
            float distance = duree * 0.0343f / 2.0f;

            Serial.print("[HC-SR04] Distance : ");
            Serial.print(distance, 2);
            Serial.println(" cm");
        }

        // Attendre 5 secondes avec FreeRTOS
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Mesure de la température et de l'humidité
void tacheDHT(void *parameter) {
    dht.begin();

    while (true) {
        float temperature = dht.readTemperature();
        float humidite = dht.readHumidity();

        if (isnan(temperature) || isnan(humidite)) {
            Serial.println("[DHT11] Erreur de lecture");
        } else {
            Serial.print("[DHT11] Temperature : ");
            Serial.print(temperature, 1);
            Serial.print(" C | Humidite : ");
            Serial.print(humidite, 1);
            Serial.println(" %");
        }

        // Attendre 5 secondes avec FreeRTOS
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== DEMARRAGE DES TACHES FREERTOS ===");

    // Créer la tâche du HC-SR04
    xTaskCreate(
        tacheUltrason,       // Fonction de la tâche
        "TacheUltrason",     // Nom
        4096,                // Taille de pile
        NULL,                // Paramètre
        1,                   // Priorité
        NULL                 // Handle
    );

    // Créer la tâche du DHT11
    xTaskCreate(
        tacheDHT,
        "TacheDHT",
        4096,
        NULL,
        1,
        NULL
    );
}

void loop() {
    // Les tâches FreeRTOS exécutent le travail.
    vTaskDelay(pdMS_TO_TICKS(1000));
}