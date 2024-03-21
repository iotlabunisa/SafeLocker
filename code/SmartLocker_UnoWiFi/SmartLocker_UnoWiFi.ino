/*
PROGRAMMA SLAVE I2C E PUBSUB SU MQTT 
ARDUINO REV2
*/

#include <Wire.h>
#include "Servo.h"
#include <DHT.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>

// DICHIARAZIONE DATI WiFi LabOfIoT
char ssid[] = "WiFi-LabIoT";
char pass[] = "s1jzsjkw5b";
WiFiClient espClient;

// DICHIARAZIONE DATI MQTT
const char* mqttServer = "192.168.1.21"; // SERVER LABORATORIO DI IoT 
const int mqttPort = 1883; // PORTA STANDARD
const char* mqttUser = ""; // LASCIARE VUOTO SE NON SERVE
const char* mqttPassword = ""; // LASCIARE VUOTO SE NON SERVE
PubSubClient client(espClient); // DICHIARAZIONE CLIENT PUBSUB

// DICHIARAZIONE SERVOMOTORE
Servo doorServo;
#define servoPin 3

// DICHIARAZIONE SENSORE TEMPERATURA
#define DHT22_PIN 2
DHT dht(DHT22_PIN, DHT22);

// DICHIARAZIONE VARIABILI
String pswState = "";
int dimensionePsw = 0;
int measure;
bool firstStrike = true; 

// DICHIARAZIONE VARIABILI NODERED
String statoImp = "ATTESA";
String statoPsw = "ATTESA";
String statoDoor = "CHIUSA";
String statoAlarm = "ALLARME SPENTO";
String admResetState = "RST_NO";

String prec_statoImp = "";
String prec_statoPsw = "";
String prec_statoDoor = "";
String prec_statoAlarm = "";
String controlPsw = "";

// DICHIARAZIONE VARIABILI PER I2C
String receivedFunction;
String receivedMessage;

// SETUP DELL'ARDUINO
void setup() {
  Wire.begin(8);                // Inizializza la comunicazione I2C e assegna l'indirizzo 8
  Wire.onReceive(receiveData);  // Definisce la funzione callback per gestire la ricezione dal master
  Wire.onRequest(sendData);     // Funzione callback per inviare al master
  Serial.begin(9600);           // Inizializzazione Seriale
  connectToWiFi();              // Connessione WiFi
  client.setServer(mqttServer, mqttPort); // Dichiarazione Server MQTT
  client.setCallback(callback); // Dichiarazione Funzione di CallBack
  connectToMQTT();              // Connetti al Server MQTT
  dht.begin();                  // Inizializza Sensore Temperatura
  doorServo.attach(servoPin);   // Inizializzazione ServoMotore
  closeMotor();                 // ServoMotore su Chiuso
  resetSituation();             // Reset MQTT
  Serial.println("INIZIALIZZAZIONE COMPLETATA.");
  delay(1000);
}

// VOID DI CONNESSIONE ALLA RETE WiFi
void connectToWiFi() {
  Serial.print("Connessione alla rete WiFi ");
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Connessione WiFi stabilita");
  Serial.print("Indirizzo IP: ");
  Serial.println(WiFi.localIP());
}

// VOID DI CONNESSIONE AL SERVER MQTT
void connectToMQTT() {
  if (!client.connected()) {
    Serial.print("Tentativo di connessione al server MQTT ");
    while (!client.connect("ArduinoClient", mqttUser, mqttPassword)) {
      Serial.print(".");
      delay(2500);
    }
    if (client.connected()) {
      Serial.println("");
      Serial.println("Connesso al server MQTT.");
      client.subscribe("secureBox_pswResult");
      client.subscribe("secureBox_resetAdmin");
    } else {
      Serial.println("Connessione MQTT fallita.");
    }
  }
}

// DICHIARAZIONE FUNZIONE DI CALLBACK MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Messaggio dal Broker MQTT [");
  Serial.print(topic);
  Serial.print("] ");
  String msgRicevuto = "";
  for (int i = 0; i < length; i++) {
    msgRicevuto += (char)payload[i];
  }
  Serial.println(msgRicevuto);
  if(firstStrike) {
    firstStrike = false;
    Serial.println("Messaggio dal Broker MQTT ignorato perchè siamo in avvio.");
    return;
  }
  if(strcmp(topic, "secureBox_pswResult") == 0) {
    Serial.println("pswState aggiornato;");
    pswState = msgRicevuto;
  }
  if(strcmp(topic, "secureBox_resetAdmin") == 0) {
    Serial.println("INVOCATO RESET DELL'ADMIN !!!!!!!!!");
    admResetState = "RST_OK"; 
  }
}

// LOOP DEL SISTEMA
void loop() {
  float temperature = dht.readTemperature(); // LETTURA TEMPERATURA E UMIDITA' DAL SENSORE DHT
  float humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) { // SE SENSORE DA ERRORE
    Serial.println("Errore nella lettura del sensore DHT!");
  } else { // SE SENSORE NON DA ERRORE 
    inviaMQTT_NodeRed("secureBox_temperatura", String(temperature));
    inviaMQTT_NodeRed("secureBox_umidita", String(humidity));
  }
  if(statoImp != prec_statoImp) {
    inviaMQTT_NodeRed("secureBox_impronta", statoImp);
    prec_statoImp = statoImp;
  }
  if(statoPsw != prec_statoPsw) {
    inviaMQTT_NodeRed("secureBox_pswCheck", statoPsw);
    prec_statoPsw = statoPsw;
  }
  if(statoDoor != prec_statoDoor) {
    inviaMQTT_NodeRed("secureBox_porta", statoDoor);
    prec_statoDoor = statoDoor;
  }
  if(statoAlarm != prec_statoAlarm) {
    inviaMQTT_NodeRed("secureBox_allarme", statoAlarm); 
    prec_statoAlarm = statoAlarm;
  }
  client.loop(); // MANTIENI CONNESSIONE MQTT ATTIVA
  delay(1000);
}

// PUBBLICA SU NODERED
void inviaMQTT_NodeRed(String mqttTopic, String value) {
  if (client.connected()) {
    client.publish(mqttTopic.c_str(), value.c_str());
    delay(250);
  } else {
    Serial.println("Connessione MQTT non attiva. Tentativo di riconnessione...");
    connectToMQTT(); // SE LA CONNESSIONE E' CADUTA PROVA A RICONNETTERE
  }
}

// RICEVI DATI I2C
void receiveData(int byteCount) {
  int dimData = 16;
  if (dimensionePsw > 0) dimData = dimensionePsw + 10;
  char receivedData[dimData];
  int index = 0;
  while (Wire.available()) {
    receivedData[index++] = Wire.read();
  }
  receivedData[index] = '\0';
  String receivedString = byteArrayToString(receivedData, sizeof(receivedData));
  receivedFunction = extractField(receivedString, '-', 0);
  receivedMessage = extractField(receivedString, '-', 1);
  Serial.print("I2C - Messaggio Ricevuto dal Server: ");
  Serial.print("Funzione: ");
  Serial.print(receivedFunction);
  Serial.print("   Msg: ");
  Serial.println(receivedMessage);
  if (dimensionePsw > 0) {
    dimensionePsw = 0;
    dimData = 16;
  }
  receiveFramework_slv(receivedFunction, receivedMessage);
}

// INVIA DATI I2C
void sendData() {
  String response = "";
  if(strcmp(receivedFunction.c_str(), "PSW_CHECK") == 0) {
    response = String("PSW_CHECK-") + String(pswState);
  }
  else if(strcmp(receivedFunction.c_str(), "ALM_CHECK") == 0) {
    if(strcmp(receivedMessage.c_str(), "ALM_ON") == 0) {
      response = String("ADM_RESET-") + String(admResetState);
      if(strcmp(admResetState.c_str(), "RST_OK") == 0) {
        admResetState = "RST_NO";
        resetSituation();
      }
    }
  }
  byte byteResponse[16];
  stringToByteArray(response, byteResponse, sizeof(byteResponse));
  Wire.write(byteResponse, sizeof(byteResponse));
  Serial.print("I2C - Messaggio Inviato al Server: ");
  Serial.println(response);
}

void receiveFramework_slv(String funzione, String messaggio) {
  // FRAMEWORK, MESSAGGI SUPPORTATI
  // 1 - IMP_CHECK - IMPRONTA
  // 2 - PSW_CHECK - PASSWORD
  // 3 - DOR_CHECK - PORTA
  // 6 - ALM_CHECK - ALLARME SONORO
  // 7 - RST_CHECK - RESET
  // 8 - MOT_OPENC - CONTROLLO MOTORE

  // FRAMEWORK, MESSAGGI POSSIBILI
  // 1 - IMP_OK IMP_ER
  // 2 - PSW
  // 3 - DOR_OP DOR_CL
  // 6 - ALM_ON ALM_OF
  // 7 - RST_OK
  // 8 - MOT_ON, MOT_OF
  if(funzione.equals("IMP_CHECK")) {
    if(messaggio.equals("IMP_OK")) {
      statoImp = "RICONOSCIUTA";
    }
    else if (messaggio.equals("IMP_ER")) {
      statoImp = "NON RICONOSCIUTA";
    }
  }
  if(funzione == "PSW_CHECK") {
    statoPsw = messaggio;
  }
  if(funzione == "PSW_DIMEN") {
    dimensionePsw = messaggio.toInt();
  }
  if(funzione == "DOR_CHECK") {
    if(messaggio == "DOR_OP") {
      statoDoor = "APERTA";
    }
    else if (messaggio == "DOR_CL") {
      statoDoor = "CHIUSA";
    }
  }
  if(funzione == "ALM_CHECK") {
    if(messaggio == "ALM_ON") {
      statoAlarm = "ALLARME IN CORSO";
    }
    if(messaggio == "ALM_OF") {
      statoAlarm = "ALLARME SPENTO";
    }
  }
  if(funzione == "RST_CHECK") {
    if(messaggio == "RST_OK") {
      resetSituation();
    }
  }
  if(funzione == "MOT_OPENC") {
    if(messaggio == "MOT_ON") {
      openMotor();
    }
    if(messaggio == "MOT_OF") {
      closeMotor();
    }
  }
}

void resetSituation() {
  statoImp = "ATTESA";
  statoPsw = "ATTESA";
  statoDoor = "CHIUSA";
  statoAlarm = "ALLARME SPENTO";
  admResetState = "RST_NO";
  pswState = "";
}

void openMotor() {
  doorServo.write(90);
  Serial.println("MOTORE SU APERTO.");
}

void closeMotor() {
  doorServo.write(180);
  Serial.println("MOTORE SU CHIUSO.");
}

/* **********
FINE DELLA LOGICA DEL PROGRAMMA
ORA FUNZIONI DI UTILITA'
********** */

// FUNZIONE CHE ESTRAE UN CAMPO DA UNA STRINGA IN BASE AL SEGNO -
String extractField(String input, char separator, int index) {
  int separatorCount = 0;
  int startIndex = 0;
  for (int i = 0; i <= input.length(); i++) {
    if (input[i] == separator || i == input.length()) {
      separatorCount++;
      if (separatorCount == index + 1) {
        return input.substring(startIndex, i);
      }
      startIndex = i + 1;
    }
  }
  return "";
}

// CONVERSIONE ARRAY DI BYTE IN STRINGA
String byteArrayToString(byte* array, int length) {
  String result = "";
  for (int i = 0; i < length; i++) {
    result += char(array[i]);
  }
  return result;
}

// CONVERSIONE STRINGA IN ARRAY DI BYTE
void stringToByteArray(String input, byte* output, int maxLength) {
  for (int i = 0; i < maxLength && i < input.length(); i++) {
    output[i] = input.charAt(i);
  }
}