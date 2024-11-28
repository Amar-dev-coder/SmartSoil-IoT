#include <Arduino.h>
#include <FirebaseESP8266.h>

// Firebase podaci i podaci Wi-fi mreže
#define WIFI_SSID "37D297 - 2.4GHz"
#define WIFI_PASSWORD "ie8fdg3tfy"
#define DATABASE_URL "sistem-vlaznosti-tla-default-rtdb.firebaseio.com"
#define API_KEY "AIzaSyAWOEseA4Kwn-whFoJfjYtFS0xx5_W7VOc"

// Firebase objekti
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// Deklarisanje varijabli i pinova
int green = 5;
int blue = 4;
int red = 0;
int buzzer = 13;
int sensorPin = A0;
int sensorValue = 0;
int procentValue = 0;
int relayPin = 2;

// Varijabla za praćenje statusa zvuka
bool buzzerOn = false;

void setup() {
  pinMode(green, OUTPUT);
  pinMode(blue, OUTPUT);
  pinMode(red, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(relayPin, OUTPUT);  

  Serial.begin(115200);

  // Inicijalizacija Wi-Fi konekcije
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Povezivanje na Wi-Fi");
  //Provjerava status dok se ne poveže
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  //Povezivanje uspješno
  Serial.println("\nPovezano na Wi-Fi!");
  Serial.print("IP adresa: ");
  Serial.println(WiFi.localIP());

  // Konfiguracija Firebase baze
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase autentifikacija uspješna!");
    signupOK = true;
  } else {
    Serial.print("Greška pri prijavi: ");
    Serial.println(config.signer.tokens.error.message.c_str());
  }
}

void loop() {
  // Čitanje senzora za vlažnost tla analogno
  sensorValue = analogRead(sensorPin);

  // Pretvaranje analognog očitavanja senzora (0-1023) u postotak vlage (0-100)
// map funkcija omogućava skaliranje očitanja u željeni raspon
  int moistureLevel = map(sensorValue, 1023, 450, 0, 100);


 //Ograničavanje da vrijednost ne može preći preko 100% i ispod 0%
  if (moistureLevel > 100) moistureLevel = 100;
  if (moistureLevel < 0) moistureLevel = 0;

  // Ispis na serijski monitor postotka vlažnosti tla
  Serial.print("Vrijednost vlažnosti tla: ");
  Serial.print(moistureLevel);
  Serial.println("%");

  // Slanje vrijednosti vlažnosti tla na Firebase
  if (signupOK) {
    if (Firebase.setInt(fbdo, "/moistureLevel", moistureLevel)) {
      Serial.println("Podaci uspješno poslani!");
    } else {
      Serial.print("Greška pri slanju podataka: ");
      Serial.println(fbdo.errorReason());
    }
  }

 /*
 Upravljanje pumpom:
 - Automatski režim: pumpa se aktivira ako nivo vlage padne ispod 40%.
 - Manuelni režim: status pumpe se očitava iz Firebase baze ("on" ili "off").
 - Ako mod nije prepoznat, pumpa ostaje neaktivna.
*/
  String mode = "automatic";  // Podrazumijevano stanje ako čitanje ne uspije
  if (Firebase.getString(fbdo, "/mode")) {
    mode = fbdo.stringData();
    Serial.print("Trenutni mod: ");
    Serial.println(mode);
  } else {
    Serial.print("Greška pri čitanju moda: ");
    Serial.println(fbdo.errorReason());
  }

  if (mode == "automatic") {
    /* Automatsko upravljanje pumpom. 
    Koristio sam obrnutu logiku za aktiviranje relaya, zbog toga 
    što je pumpa spojena na NC (Normal close). Prilikom spajanja na 
    NC, pri početnom stanju relay koristi otvoreno kolo. 
    */
    if (moistureLevel <= 40) {
      digitalWrite(relayPin, LOW);  // Aktiviraj pumpu
      Serial.println("Pumpa automatski uključena (nizak nivo vlage)!");
    } else {
      digitalWrite(relayPin, HIGH);  // Isključi pumpu
      Serial.println("Pumpa automatski isključena (dovoljan nivo vlage)!");
    }
  } else if (mode == "manual") {
    // Manuelno upravljanje pumpom putem Firebase
    if (Firebase.getString(fbdo, "/pump")) {
      String relayStatus = fbdo.stringData();
      if (relayStatus == "on") {
        digitalWrite(relayPin, LOW);  // Aktiviraj pumpu
        Serial.println("Pumpa manuelno uključena!");
      } else {
        digitalWrite(relayPin, HIGH);  // Isključi pumpu
        Serial.println("Pumpa manuelno isključena!");
      }
    } else {
      Serial.print("Greška pri čitanju statusa pumpe: ");
      Serial.println(fbdo.errorReason());
    }
  } else {
    Serial.println("Nepoznat mod. Pumpa ostaje neaktivna.");
  }

  /*
 LED indikatori:
 - Crvena LED: Vlažnost tla ispod 40%.
 - Plava LED: Vlažnost tla između 40% i 60%.
 - Zelena LED: Vlažnost tla iznad 60%.
  Alarm se također oglašava da vlažnost tla pane ispod 40%
*/
  if (moistureLevel <= 40) {
    digitalWrite(green, LOW);
    digitalWrite(blue, LOW);
    digitalWrite(red, HIGH);
    if (!buzzerOn) {
      tone(buzzer, 1400);
      buzzerOn = true;
      Firebase.setString(fbdo, "/buzzerStatus", "on");
    }
  } else {
    noTone(buzzer);
    if (buzzerOn) {
      buzzerOn = false;
      Firebase.setString(fbdo, "/buzzerStatus", "off");
    }
    if (moistureLevel <= 60) {
      digitalWrite(green, LOW);
      digitalWrite(blue, HIGH);
      digitalWrite(red, LOW);
    } else {
      digitalWrite(green, HIGH);
      digitalWrite(blue, LOW);
      digitalWrite(red, LOW);
    }
  }

  delay(1000);  // Pauza između očitavanja
}
