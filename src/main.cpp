#include <Arduino.h>
#include <DHT.h>

// Sensor Pins // Broches des capteurs
#define TRIG_PIN 5
#define ECHO_PIN 18
#define DHT_PIN 15
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

// Type of measurement sent to the queue // Type de mesure envoyée dans la queue
enum TypeCapteur : uint8_t {
    ULTRASON,
    TEMPERATURE_HUMIDITE
};

// Message passed between tasks // Message transmis entre les tâches
struct Mesure {
    uint32_t tempsMs;
    TypeCapteur type;
    float distance;
    float temperature;
    float humidite;
};

//  Queue shared among tasks //Queue partagée entre les tâches
QueueHandle_t fileMesures = nullptr;

// Task 1: Ultrasonic Sensor //Tâche 1 : capteur ultrasonique
void tacheUltrason(void *parameter) {
    pinMode(TRIG_PIN, OUTPUT); // ici on etablie une communication entre Esp et le capteur.et out pour que l esp puisse lui permetrre de recevoir les donn'ees
    pinMode(ECHO_PIN, INPUT); // configure le role de la broche(pin mode) l'esp 32 doit lire le signal entrant
    digitalWrite(TRIG_PIN, LOW);

    while (true) {
        digitalWrite(TRIG_PIN, LOW);
        delayMicroseconds(2);
        digitalWrite(TRIG_PIN, HIGH);
        delayMicroseconds(10);
        digitalWrite(TRIG_PIN, LOW);

        unsigned long duree = pulseIn(ECHO_PIN, HIGH, 30000);

        if (duree != 0) {
            Mesure m = {};
            m.tempsMs = millis();
            m.type = ULTRASON;
            m.distance = duree * 0.0343f / 2.0f;

            // Send the measurement to the queue // Envoyer la mesure dans la queue  
            xQueueSend(fileMesures, &m, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Task 2: DHT11 Sensor // Tâche 2 : capteur DHT11 
void tacheDHT(void *parameter) {
    dht.begin();

    while (true) {
        float temperature = dht.readTemperature();
        float humidite = dht.readHumidity();

        if (!isnan(temperature) && !isnan(humidite)) {
            Mesure m = {};
            m.tempsMs = millis();
            m.type = TEMPERATURE_HUMIDITE;
            m.temperature = temperature;
            m.humidite = humidite;

            // Send the measurement to the queue //Envoyer la mesure dans la queue
            xQueueSend(fileMesures, &m, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Task 3: Serial Communication and Local Storage // Tâche 3 : communication série et stockage local 

void tacheCommunication(void *parameter) {
    static Mesure historique[100];
    size_t nombreMesures = 0;
    size_t prochainePosition = 0;
    String commande = "";

    Serial.println("Available Commands :");
    Serial.println("csv: Export measurements");
    Serial.println("help: display commands");

    while (true) {
        Mesure m;

        if (xQueueReceive(fileMesures, &m, pdMS_TO_TICKS(100)) == pdPASS) {
            historique[prochainePosition] = m;
            prochainePosition = (prochainePosition + 1) % 100;

            if (nombreMesures < 100) {
                nombreMesures++;
            }

            if (m.type == ULTRASON) {
                Serial.print("[HC-SR04] ");
                Serial.print(m.distance, 2);
                Serial.println(" cm");
            } else {
                Serial.print("[DHT11] T=");
                Serial.print(m.temperature, 1);
                Serial.print(" C, H=");
                Serial.print(m.humidite, 1);
                Serial.println(" %");
            }
        }

        // Read characters received on the serial port // Lire les caractères reçus sur le port série
        while (Serial.available() > 0) {
            char caractere = Serial.read();

            if(caractere != '\n' && caractere != '\r'){
                Serial.print(caractere);
            }

            if (caractere == '\n' || caractere == '\r') {
                Serial.println();
                commande.trim();

                if (commande.equalsIgnoreCase("help")) {
                    Serial.println("Commandes : csv, help");
                } else if (commande.equalsIgnoreCase("csv")) {
                    Serial.println( "temps_ms,capteur,distance_cm,temperature_C,humidite_pct" );

                    size_t debut =(prochainePosition + 100 - nombreMesures) % 100;

                    for (size_t i = 0; i < nombreMesures; i++) {
                        const Mesure &r = historique[(debut + i) % 100];

                        Serial.print(r.tempsMs);
                        Serial.print(",");

                        if (r.type == ULTRASON) {
                            Serial.print("HC-SR04,");
                            Serial.print(r.distance, 2);
                            Serial.println(",,");
                        } else {
                            Serial.print("DHT11,,");
                            Serial.print(r.temperature, 1);
                            Serial.print(",");
                            Serial.println(r.humidite, 1);
                        }
                    }
                    Serial.println("# Fin CSV");
                } else if (commande.length() > 0) {
                    Serial.println("Commande inconnue. Tapez help.");
                }

                commande = "";
            } else{
                if(commande.length() < 20){
                    commande = commande + caractere;
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(50);
    delay(1000);

    Serial.println("=== DEMARRAGE FREERTOS ===");

    // Create a queue of 20 messages // Créer une queue de 20 messages 
    fileMesures = xQueueCreate(20, sizeof(Mesure));

    if (fileMesures == nullptr) {
        Serial.println("ERROR: Unable to create the queue");
        while (true) {
            delay(1000);
        }
    }

    // Créer les trois tâches // Create the three tasks
    BaseType_t resultat1 = xTaskCreate( tacheUltrason, "TacheUltrason", 4096, nullptr, 1, nullptr );

    BaseType_t resultat2 = xTaskCreate( tacheDHT, "TacheDHT", 4096,nullptr, 1, nullptr );

    BaseType_t resultat3 = xTaskCreate( tacheCommunication, "TacheCommunication", 6144, nullptr, 1, nullptr );

    if (resultat1 != pdPASS ||resultat2 != pdPASS || resultat3 != pdPASS) // if not all tasks have been created correctly
     {
        Serial.println("ERREUR : creation d'une tache impossible");
    }
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
