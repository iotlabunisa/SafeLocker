/* **********
PROGRAMMA ARDUINO MEGA
COLLEGATI:
 - LED GREEN
 - LED RED
 - BTN RED
 - BTN GREEN
 - BUZZER
 - DOOR
 - LCD
 - KEYPAD
 - I2C
********** */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <EEPROM.h>

#define mySerial Serial1

// DICHIARAZIONE PIN 
const byte LED_GREEN_PIN = 25;
const byte LED_RED_PIN = 27;
const byte BTN_GREEN_PIN = 31;
const byte BTN_RED_PIN = 2;
const byte BUZZER_PIN = 33;
const byte DOOR_PIN = 3;

// DICHIARAZIONE TASTIERINO
const byte ROW_NUM    = 4; // quattro righe
const byte COLUMN_NUM = 4; // quattro colonne
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte pin_rows[ROW_NUM] = {36, 34, 32, 30}; // Collegherai questi ai pin 7-10
byte pin_column[COLUMN_NUM] = {28, 26, 24, 22}; // Collegherai questi ai pin 3-6
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

// DICHIARAZIONE LETTORE IMPRONTE
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// DICHIARAZIONE LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Cambia 0x27 con l'indirizzo I2C corretto per il tuo schermo

// DICHIARAZIONE VARIABILI UTILI
int tentativiImp = 5;
int tentativiPsw = 5;
bool doorLocked = true;
bool impOk = false;
bool pswOk = false;
bool login = false;
bool pswRicevuta = false;
bool adminReset_bool = false;
bool varControllo1 = false;
bool firstOn = true;

int lastImpID = 0;

// DICHIARAZIONE VARIABILI PER ATTACHINTERRUPT
volatile bool rstPressedbool = false;
volatile bool doorOpenedbool = false;
unsigned long lastDebounceTime_rst = 0;
unsigned long lastDebounceTime_door_o = 0;
unsigned long debounceDelay = 75;
int contRst = 0;
int contDoor = 0;

// SETUP DEL PROGRAMMA
void setup() {
  Wire.begin(); // DICHIARAZIONE I2C
  Serial.begin(9600); // DICHIARAZIONE SERIALE
  while (!Serial); // INIZIALIZZAZIONE SERIALE
  finger.begin(57600); // INIZIALIZZAZIONE LETTORE DI IMPRONTE
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Sensore di impronte rilevato;");
  } else {
    Serial.println("Sensore di impronte non rilevato;");
    while (1) { delay(1); }
  }
  lcd.init(); // INIZIALIZZAZIONE LCD
  lcd.begin(20, 4);
  lcd.backlight();
  lcd.clear();

  // INIZIALIZZAZIONE PIN DISPOSITIVI
  pinMode(LED_GREEN_PIN, OUTPUT); 
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_GREEN_PIN, INPUT_PULLUP);
  pinMode(BTN_RED_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT_PULLUP);

  // DICHIARAZIONE DEGLI INTERRUPT
  attachInterrupt(digitalPinToInterrupt(BTN_RED_PIN), rstPressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(DOOR_PIN), doorStatusChanged, CHANGE);

  // CARICA ULTIMO ID IMPRONTA DALLA EEPROM
  lastImpID = EEPROM.read(0);
  if (lastImpID < 1 || lastImpID > 127) {
    EEPROM.write(0, 1);
    lastImpID = 1;
    Serial.println("EEPROM non inizializzata, scrivo 1");
  }
}

// CATCH DEL PULSANTE RESET PREMUTO
void rstPressed() {
  unsigned long currentMillis = millis();
  if(currentMillis - lastDebounceTime_rst > debounceDelay) {
    lastDebounceTime_rst = currentMillis;
    rstPressedbool = true;
    contRst++;
    Serial.print("BOTTONE RESET PREMUTO: ");
    Serial.println(contRst);
  }
}

// CATCH DEL CAMBIO STATO PORTA
void doorStatusChanged() {
  unsigned long currentMillis = millis();
  if(currentMillis - lastDebounceTime_door_o > debounceDelay) {
    lastDebounceTime_door_o = currentMillis;
    if(digitalRead(DOOR_PIN) == HIGH) {
      Serial.println("PORTA APERTA");
      doorOpenedbool = true;
    }
    if(digitalRead(DOOR_PIN) == LOW) {
      Serial.println("PORTA CHIUSA");
      doorOpenedbool = false;
    }
  }
}

// PROMPT DI LOGIN
boolean faseLogin() {
  lcd.clear();
  lcd.print("     Benvenuto!     ");
  lcd.setCursor(0,1);
  lcd.print("   Secure Box n.1   ");
  lcd.setCursor(0,3);
  lcd.print(" Verde per iniziare ");
  if(doorLocked) { // INIZIALIZZAZIONE LED ROSSO E VERDE
    digitalWrite(LED_GREEN_PIN,LOW);
    digitalWrite(LED_RED_PIN,HIGH);
  } else {
    digitalWrite(LED_RED_PIN,LOW);
    digitalWrite(LED_GREEN_PIN,HIGH);
  }
  // LOOP FINCHE' NON PREMIAMO IL VERDE
  while((digitalRead(BTN_GREEN_PIN) == HIGH) && !doorOpenedbool) { 
    delay(50);
  }
  if(rstPressedbool) { // CONTROLLI DI INTERRUZIONE IN CASO DI RST E PORTA APERTA
    Serial.println("INTERROMPO OPERAZIONE, RST PREMUTO DOPO INIZIO.");
    return false;
  }
  if(doorOpenedbool) {
    Serial.println("INTERROMPO OPERAZIONE, ALLARME PORTA APERTA.");
    return false;
  }
  // FASE IMPRONTA DIGITALE
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("      Inserire      ");
  lcd.setCursor(0,1);
  lcd.print(" Impronta  Digitale ");
  // LOOP FINCHE' IMPRONTA NON E' OK E NON E' PREMUTO RESET E NON C'E' RESET ADMIN
  while(!checkImp() && !impOk && !rstPressedbool && !adminReset_bool) { 
    delay(50);
  }
  if(rstPressedbool) { // CONTROLLI DI INTERRUZIONE IN CASO DI RST E PORTA APERTA E ADMIN RESET
    Serial.println("INTERROMPO OPERAZIONE, RST PREMUTO DOPO IMPRONTA.");
    return false;
  }
  if(doorOpenedbool) {
    Serial.println("INTERROMPO OPERAZIONE, ALLARME PORTA APERTA.");
    return false;
  }
  if(adminReset_bool) {
    Serial.println("INTERROMPO OPERAZIONE, ADMIN RESET.");
    return false;
  }
  // FASE PASSWORD
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Inserire Password: ");
  lcd.setCursor(0,2);
  lcd.print("Verde per confermare");
  lcd.setCursor(0,3);
  lcd.print("* per cancellare");
  while(!checkPsw() && !pswOk && !rstPressedbool && !doorOpenedbool && !adminReset_bool) {
    delay(50);
  }
  if(rstPressedbool) { // CONTROLLI DI INTERRUZIONE IN CASO DI RST E PORTA APERTA E ADMIN RESET
    Serial.println("INTERROMPO OPERAZIONE, RST PREMUTO DOPO PSW.");
    return false;
  }
  if(doorOpenedbool) {
    Serial.println("INTERROMPO OPERAZIONE, ALLARME PORTA APERTA.");
    return false;
  }
  if(adminReset_bool) {
    Serial.println("INTERROMPO OPERAZIONE, ADMIN RESET.");
    return false;
  }
  return true;
}

// LOOP
void loop() {
  if(firstOn) { // SE PRIMA ACCENSIONE ASPETTIAMO 5 SECONDI PER INIZIALIZZARE REV2
    firstOn = false;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("  INIZIALIZZAZIONE  ");
    lcd.setCursor(0,1);
    lcd.print("      IN CORSO      ");
    lcd.setCursor(5,2);
    for(int i = 0; i < 5; i++) {
      lcd.print("- ");
      delay(1000);
    }
    lcd.setCursor(0,3);
    lcd.print("     COMPLETATO     ");
    delay(1000);
  }
  // SE NON E' PREMUTO IL RESET E NON E' APERTA LA PORTA E NON C'E' LOGIN
  if(!rstPressedbool && !doorOpenedbool && !login) {
    login = faseLogin();
  }
  if(!login) {
    Serial.println("Login Fallita.");
  }
  if(!login && doorOpenedbool) { // SE LA LOGIN NON E' FATTA E LA PORTA E' APERTA
    comunicaPorta();
    allarmeInfinito();
  }
  if(login && !rstPressedbool) { // SE LA LOGINE E' FATTA E NON E' PREMUTO IL RESET
    if(!varControllo1) { 
      // SE LA LOGIN E' FATTA E IL BOTTONE DI RESET NON RISULTA PREMUTO
      // SIAMO NEL PUNTO IN CUI POSSIAMO APRIRE LA PORTA, QUINDI SBLOCCHIAMO IL SERVOMOTORE
      // LA VAR CONTROLLO MI SERVE PER NON RIPETERE AD OGNI LOOP QUESTA OPERAZIONE
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, HIGH); 
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("    PREGO APRIRE    ");
      lcd.setCursor(0,2);
      lcd.print("       PORTA.       ");     
      sendFramework_srv(8, "MOT_ON");
      while(!doorOpenedbool && !rstPressedbool) { // LOOP FINCHE' PORTA NON APERTA E RST NON PREMUTO
        delay(50);
      }
      comunicaPorta();
    }
    varControllo1 = true;
  }
  if(login && doorOpenedbool) { // SE LOGIN E' FATTO E PORTA E' APERTA
    if(rstPressedbool) {
      // SE LA LOGIN E' FATTA E LA PORTA E' APERTA MA VIENE PREMUTO IL RESET
      // DOBBIAMO AVVISARE DI CHIUDERE LA PORTA
      // FINCHE' NON VERRA CHIUSA CI SARA' UN ALLARME PIU' TENUE 
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("   ATTENZIONE!!!!   ");
      lcd.setCursor(0,1);
      lcd.print(" CHIUDERE LA PORTA! ");
      lcd.setCursor(0,2);
      lcd.print("   PER CONCLUDERE   ");
      while(doorOpenedbool) { // LOOP FINCHE' NON SI CHIUDE LA PORTA
        tonoBreve();
        delay(50);
      }
      comunicaPorta();
    }
  }
  if (rstPressedbool && !doorOpenedbool) {
    // SE VIENE PREMUTO IL BOTTONE DI RESET E LA PORTA E' CHIUSA
    // CHIUDIAMO IL SERVO CHE BLOCCA LA PORTA
    sendFramework_srv(8, "MOT_OF");
    comunicaPorta();
    // RESET DI IMPRONTA E PASSWORD INSERITE E TUTTE LE VARIABILI DI CONTROLLO
    // NON RESETTIAMO I TENTATIVI PER EVITARE TENTATIVI INFINITI
    impOk = false;
    pswOk = false;
    rstPressedbool = false;
    login = false;
    varControllo1 = false;
    pswRicevuta = false;
    sendFramework_srv(7, "RST_OK");
    Serial.println("RESET AVVENUTO, VARIABILI AZZERATE (TRANNE I TENTATIVI)");
  }
  if(adminReset_bool) { // SE VIENE EFFETTUATO IL RESET DALL'ADMIN
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("   ATTENZIONE!!!!   ");
    lcd.setCursor(0,1);
    lcd.print(" CHIUDERE LA PORTA! ");
    lcd.setCursor(0,2);
    lcd.print("   PER CONCLUDERE   ");
    while(doorOpenedbool) { // LOOP FINCHE' NON VIENE CHIUSA LA PORTA
      tonoBreve();
      delay(50);
    }
    comunicaPorta();
    sendFramework_srv(8, "MOT_OF");
    impOk = false;
    pswOk = false;
    tentativiPsw = 5;
    tentativiImp = 5;
    rstPressedbool = false;
    login = false;
    varControllo1 = false;
    pswRicevuta = false;
    adminReset_bool = false;
    sendFramework_srv(7, "RST_OK");
    Serial.println("RESET ADMIN AVVENUTO, TUTTE LE VARIABILI RESETTATE.");
  }
  delay(250); // DELAY FINALE PER NON ANDARE TROPPO VELOCI ALL'INIZIO DEL LOOP
}

//COMUNICAZIONE PORTA APERTA/CHIUSA
void comunicaPorta() {
  if(doorOpenedbool) {
    //COMUNICARE PORTA APERTA
    sendFramework_srv(3, "DOR_OP");
  }
  else {
    //COMUNICARE PORTA CHIUSA
    sendFramework_srv(3, "DOR_CL");
  }
}

// ALLARME FORTE
void allarmeInfinito() {
  // AVVISO ARDUINO REV2 E PARTE L'ALLARME
  sendFramework_srv(6, "ALM_ON");
  lcd.clear();
  while(true && !adminReset_bool) { // LOOP FINCHE' ADMIN NON RESETTA
    lcd.setCursor(0,0);
    lcd.print("   Secure Box n.1   ");
    lcd.setCursor(0,1);
    lcd.print("TENTATIVO INTRUSIONE");
    lcd.setCursor(0,2);
    lcd.print(" GUARDIE IN ARRIVO! ");
    for(int i = 0; i < 3; i++) {
     tone(BUZZER_PIN, 2000, 250);
     delay(250);
     noTone(BUZZER_PIN);
     delay(250); 
    }
    adminReset();
  }
}

// ALLARME TENTATIVI ESAURITI
void allarmeTentativiEsauriti() {
  // AVVISO ARDUINO REV2 E PARTE L'ALLARME
  sendFramework_srv(6, "ALM_ON");
  lcd.clear();
  while(true && !adminReset_bool) { // LOOP FINCHE' ADMIN NON RESETTA
    lcd.setCursor(0,0);
    lcd.print("   Secure Box n.1   ");
    lcd.setCursor(0,1);
    lcd.print("TENTATIVI ESAURITI!!");
    lcd.setCursor(0,2);
    lcd.print("CONTATTATO UN ADMIN!");
    for(int i = 0; i < 3; i++) { // BUZZER 3 VOLTE
     tone(BUZZER_PIN, 1300, 250);
     delay(250);
     noTone(BUZZER_PIN);
     delay(250); 
    }
    adminReset();
    Serial.print("ALLTENTESAU: ");
    Serial.println(adminReset_bool);
  }
}

void adminReset() {
  delay(3000); // DELAY PER NON COMUNICARE TROPPO VELOCEMENTE CON REV2
  bool tempOk = receiveDataFromSlave("ADMIN"); // RICHIESTA AL REV2 SE C'E' RESET
  if(tempOk) { 
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("RICEVUTO");
    lcd.setCursor(0,1);
    lcd.print("RESET");
    lcd.setCursor(0,2);
    lcd.print("AMMINISTRATORE");
    adminReset_bool = true;
    delay(1000);
  }
}

// TONO PIU' TENUE E BREVE PER PICCOLI AVVISI
void tonoBreve() {
  tone(BUZZER_PIN, 1300, 250);
  delay(250);
  noTone(BUZZER_PIN);
  delay(250);
}

// INSERIMENTO NUOVA IMPRONTA 
void programmaNewImp() {
  Serial.println("PRONTI ALL'INSERIMENTO");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("INSERIMENTO IMPRONTA");
  lcd.setCursor(0,1);
  if (lastImpID == 0) {
    Serial.println("ERRORE, LETTURA VALORE DALLA EEPROM");
    lcd.print("ERRORE:LETTURA EEPROM");
    delay(1000);
    return;
  }
  if (lastImpID >= 127) {
    Serial.println("ERRORE, SENSORE PIENO.");
    lcd.print("ERRORE:MEMORIA PIENA");
    delay(1000);
    return;
  }
  lastImpID = lastImpID + 1;
  Serial.print("ID IMPRONTA ASSEGNATO: ");
  Serial.println(lastImpID);
  lcd.print("ID ASSEGNATO: ");
  lcd.print(lastImpID);
  lcd.setCursor(0,2);
  while(!getFingerprintEnroll(lastImpID) && !rstPressedbool);
  EEPROM.write(0, lastImpID);
}

// CHECK DELLA PASSWORD 
boolean checkPsw(){
  lcd.setCursor(0,1);
  String entered_code = "";
  while(digitalRead(BTN_GREEN_PIN) == HIGH) {
    if(rstPressedbool) { // CONTROLLO DI RESET E PORTA 
      Serial.println("INTERROMPO OPERAZIONE, RESET PREMUTO.");
      return false;
    }
    if(doorOpenedbool) {
      Serial.println("INTERROMPO OPERAZIONE, PORTA APERTA.");
      return false;
    }
    char key = keypad.getKey();
    if(key) {
      entered_code += key;
      lcd.print("*");
      if(key == '*') { // SE SI PREME * CANCELLARE STRINGA INSERITA
        entered_code = "";
        lcd.setCursor(0,1);
        lcd.print("                    ");
        lcd.setCursor(0,1);
      }
    }
  }
  Serial.print("Password Inserita: ");
  Serial.println(entered_code);
  if(strcmp(entered_code.c_str(), "BA#123") == 0) {
    Serial.println("ENTRATI IN MODALITA' PROGRAMMAZIONE IMPRONTA.");
    programmaNewImp();
    Serial.println("USCITI DA MODALITA' PROGRAMMAZIONE IMPRONTA, RESET IN CORSO.");
    rstPressedbool = true;
    return false;
  }
  sendFramework_srv(2, entered_code); // INVIO AL REV2 PER IL CONTROLLO
  Serial.print("Inizio attesa password:");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" CONTROLLO PASSWORD ");
  lcd.setCursor(0,1);
  lcd.print("      IN CORSO      ");
  lcd.setCursor(4,2);
  for(int i = 0; i < 5; i++){ // LOOP PER FAR PASSARE 3,5 SECONDI
    Serial.print(" -");
    lcd.print(" -");
    delay(700);
  }
  pswOk = receiveDataFromSlave("PASSWORD"); // RICEVO LA RISPOSTA DAL REV2
  Serial.println(" Attesa Finita;");
  lcd.setCursor(0,3);
  lcd.print("  ATTESA  CONCLUSA  ");
  delay(1000);
  if(pswOk) { // SE LA PASSWORD CORRISPONDE
    Serial.println("La Password Corrisponde;");
    tentativiPsw = 5;
    return true;
  }
  else { // SE LA PASSWORD E' SBAGLIATA
    Serial.println("La Password è sbagliata;");
    tentativiPsw--;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("      ERRORE!!      ");
    lcd.setCursor(0,1);
    lcd.print("Password Errata");
    lcd.setCursor(0,2);
    lcd.print("Ancora ");
    lcd.print(tentativiPsw);
    lcd.print(" tentativi.");
    if(tentativiPsw == 0) { // SE TENTATIVI ESAURITI
      Serial.println("Tentativi massimi raggiunti, attivo allarme;");
      allarmeTentativiEsauriti();
      return false; // SERVE PER RITORNARE IL CONTROLLO AL LOOP
    }
    delay(2000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(" Inserire Password: ");
    lcd.setCursor(0,2);
    lcd.print("Verde per confermare");
    lcd.setCursor(0,3);
    lcd.print("* per cancellare");
    return false; // SERVER PER RITORNARE IL CONTROLLO AL LOOP
  }
}

// CHECK DELL'IMPRONTA
boolean checkImp() {
  // LEGGI L'IMPRONTA, QUESTO E' UN WHILE, IMPOSSIBILE FARE CONTROLLI DURANTE QUESTO
  // PORTA RESET ECC' VERRANNO CONTROLLATI DOPO
  uint8_t p = finger.getImage(); 
  if(p != FINGERPRINT_NOFINGER){
    Serial.println("Impronta acquisita;");
    p = finger.image2Tz();
    if(p == FINGERPRINT_OK){
      p = finger.fingerSearch();
      if(p == FINGERPRINT_OK) { // SE TROVO L'IMPRONTA
        Serial.print("Impronta trovata con id: ");
        Serial.println(finger.fingerID);
        tentativiImp = 5;
        sendFramework_srv(1,"IMP_OK"); // AVVISO REV2 CHE IMPRONTA E' OK
        lcd.setCursor(0,2);
        lcd.print("                    ");
        lcd.setCursor(0,2);
        lcd.print("  Impronta Trovata  ");
        lcd.setCursor(0,3);
        lcd.print("                    ");
        lcd.setCursor(0,3);
        lcd.print("       ID: ");
        lcd.print(finger.fingerID);
        delay(1500);
        impOk = true;
        return true;
      }
      else if (p == FINGERPRINT_NOTFOUND) { // SE NON TROVO L'IMPRONTA
        tentativiImp--;
        if(tentativiImp < 0){
          tentativiImp = 0;
        }
        lcd.setCursor(0,2);
        lcd.print("Impronta non trovata");
        lcd.setCursor(0,3);
        lcd.print("Tentativi rimasti: ");
        lcd.print(tentativiImp);
        Serial.print("Impronta non trovata, rimangono ");
        Serial.print(tentativiImp);
        Serial.println(" tentativi;");
        sendFramework_srv(1,"IMP_ER"); // AVVISO REV2 CHE IMPRONTA NON E' OK
        if (tentativiImp == 0) { // SE HO FINITO I TENTATIVI
          Serial.println("Tentativi massimi raggiunti, attivo allarme;");
          allarmeTentativiEsauriti();
          Serial.print("CHECKIMP: ");
          Serial.println(adminReset_bool);
          return false;
        }
      }
    }
  }
  return false;
}

// FRAMEWORK DI INVIO AL REV2
void sendFramework_srv(int nFunction, String dataMessage) {
  // FRAMEWORK, MESSAGGI SUPPORTATI (nFunction)
  // 1 - IMP_CHECK - IMPRONTA
  // 2 - PSW_CHECK - PASSWORD
  // 3 - DOR_CHECK - PORTA
  // 6 - ALM_CHECK - ALLARME SONORO
  // 7 - RST_CHECK - RESET
  // 8 - MOT_OPENC - APRI CHIUDI MOTORE

  // FRAMEWORK, MESSAGGI POSSIBILI
  // 1 - IMP_OK IMP_ER MESSO
  // 2 - PSW
  // 3 - DOR_OP DOR_CL
  // 6 - ALM_ON ALM_OF
  // 7 - RST_OK
  // 8 - MOT_ON, MOT_OFF
  
  Serial.print("Invocato invio al REV2 - Funzione: ");
  Serial.print(nFunction);
  Serial.print(";   Messaggio: ");
  Serial.print(dataMessage);
  Serial.println(";");
  
  String msgFinale = "";
  int defaultLen = 16;
  if(nFunction == 1) { // ESECUZIONE INVIO IMPCHECK
    msgFinale = String("IMP_CHECK") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 2) { // ESECUZIONE INVIO PASSWORD LENGTH E PASSWORD CHECK
    String dimString = String(dataMessage.length());
    padZerosLeft(dimString, 6);
    String msgFin1 = String("PSW_DIMEN") + "-" + dimString;
    String msgFin2 = String("PSW_CHECK") + "-" + dataMessage;
    performSend(msgFin1, msgFin1.length());
    performSend(msgFin2, msgFin2.length());
  }
  else if(nFunction == 3) { // ESECUZIONE INVIO DOOR CHECK
    msgFinale = String("DOR_CHECK") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 6) { // ESECUZIONE INVIO ALARM CHECK
    msgFinale = String("ALM_CHECK") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 7) { // ESECUZIONE INVIO RST CHECK
    msgFinale = String("RST_CHECK") + "-" + "RST_OK";
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 8) { // ESECUZIONE INVIO APRIRE MOTORE
    msgFinale = String("MOT_OPENC") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else { // IN CASO DI FUNZIONE ERRATA
    Serial.println("Errore: E' stata richiesta una funzione non valida.");
  }
}

// ATTO DI INVIO AL REV2
void performSend(String dataToSend, int dataLen) {
  byte byteData[dataLen];
  stringToByteArray(dataToSend, byteData, sizeof(byteData));
  Wire.beginTransmission(8);
  Wire.write(byteData, sizeof(byteData));
  Wire.endTransmission();
  Serial.print("I2C - Inviato Messaggio allo Slave: ");
  Serial.println(dataToSend);
}

// FRAMEWORK DI RICEZIONE DAL REV2
boolean receiveFramework_srv(String function, String dataMessage) {
  // DATO CHE FUNZIONA TUTTO TRAMITE CONTROLLI
  // QUESTO FRAMEWORK RITORNA SOLAMENTE TRUE O FALSE
  if(function == "PSW_CHECK") {
    if(strcmp(dataMessage.c_str(), "PSW_OK") == 0) {
      return true;
    }
    else if(strcmp(dataMessage.c_str(), "PSW_ER") == 0) {
      return false;
    }
    else {
      return false;
    }
  }
  else if(function == "ADM_RESET") {
    if(strcmp(dataMessage.c_str(), "RST_OK") == 0) {
      return true;
    }
    else if(strcmp(dataMessage.c_str(), "RST_NO") == 0) {
      return false;
    }
  }
  else {
    return false;
  }
}

// ATTO DI RICEZIONE DAL REV2
// RITORNA TRUE O FALSE IN BASE AL RITORNO DEL FRAMEWORK
boolean receiveDataFromSlave(String func) {
  Wire.requestFrom(8, 16);
  byte receivedData[16];
  int index = 0;
  while (Wire.available()) {
    receivedData[index++] = Wire.read();
  }
  String receivedString = byteArrayToString(receivedData, sizeof(receivedData));
  String receivedFunction = extractField(receivedString, '-', 0);
  String receivedMessage = extractField(receivedString, '-', 1);
  Serial.print("I2C - Ricevuto dallo Slave: Funzione: ");
  Serial.print(receivedFunction);
  Serial.print("; Messaggio dallo Slave: ");
  Serial.print(receivedMessage);
  Serial.println(";");
  if(strcmp(func.c_str(), "ADMIN") == 0) { // DATO CHE ABBIAMO SOLO DUE FUNZIONI LE CHIAMO COSI'
    if(strcmp(receivedFunction.c_str(), "ADM_RESET") == 0) {
      return receiveFramework_srv(receivedFunction, receivedMessage);
    }
    else return false;
  }
  else if(strcmp(func.c_str(), "PASSWORD") == 0) {
    if(strcmp(receivedFunction.c_str(), "PSW_CHECK") == 0) {
      return receiveFramework_srv(receivedFunction, receivedMessage);
    }
    else return false;
  }
  else return false;
}

/*
LOGICA DEL PROGRAMMA FINITA
FUNZIONI DI UTILITA'
*/

// RIEMPIE DI 0 UNA STRINGA PER FARLA ARRIVARE AD UNA LUNGHEZZA FISSATA
void padZerosLeft(String &str, int fixedLength) {
  int currentLength = str.length();
  if (currentLength < fixedLength) {
    int zerosToAdd = fixedLength - currentLength;
    String zeros = "0";
    for (int i = 1; i < zerosToAdd; i++) {
      zeros += "0";
    }
    str = zeros + str;
  }
}

// ESTRAZIONE DEI FIELD FUNZIONE IN BASE AL -
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

// CONVERSIONE DA ARRAY DI BYTE A STRINGA
String byteArrayToString(byte* array, int length) {
  String result = "";
  for (int i = 0; i < length; i++) {
    result += char(array[i]);
  }
  return result;
}

// CONVERSIONE DA STRINGA AD ARRAY DI BYTE
void stringToByteArray(String input, byte* output, int maxLength) {
  for (int i = 0; i < maxLength && i < input.length(); i++) {
    output[i] = input.charAt(i);
  }
}

// AGGIUNTA DI NUOVA IMPRONTA DIGITALE
uint8_t getFingerprintEnroll(int id) {
  int p = -1;
  lcd.setCursor(0,2);
  lcd.print("POGGIARE IL DITO");
  lcd.setCursor(0,3);
  Serial.print("ATTESA IMPRONTA VALIDA PER L'ID: "); 
  Serial.println(id);
  while (p != FINGERPRINT_OK) {
    lcd.print("                    ");
    lcd.setCursor(0,3);
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("IMMAGINE RACCOLTA");
      lcd.print("IMPRONTA RACCOLTA");
      delay(1000);
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("ERRORE DI COMUNICAZIONE");
      lcd.print("ERROR COMUNICAZIONE");
      delay(1000);
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("ERRORE NELLA IMMAGINE");
      lcd.print("IMAGE FAIL");
      delay(1000);
      break;
    default:
      Serial.println("ERRORE SCONOSCIUTO");
      lcd.print("ERRORE SCONOSCIUTO");
      delay(1000);
      break;
    }
  }
  // OK success!
  p = finger.image2Tz(1);
  lcd.setCursor(0,3);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("IMMAGINE CONVERTITA");
      lcd.print("IMPRONTA CONVERTITA");
      delay(1000);
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("IMMAGINE SPORCA");
      lcd.print("IMMAGINE SPORCA");
      delay(1000);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("ERRORE DI COMUNICAZIONE");
      lcd.print("ERROR COMUNICAZIONE");
      delay(1000);
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("NON RIESCO AD ESTRARRE FEATURE DALL'IMMAGINE");
      lcd.print("ERROR FEATURE");
      delay(1000);
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("NON SONO TROVABILI FEATURE NELL'IMMAGINE");
      lcd.print("ERROR FEATURE2");
      delay(1000);
      return p;
    default:
      Serial.println("ERRORE SCONOSCIUTO");
      return p;
  }
  Serial.println("RIMUOVERE DITO");
  lcd.setCursor(0,2);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  lcd.print("                    ");
  lcd.setCursor(0,2);
  lcd.print("RIMUOVERE DITO");
  delay(1000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); 
  Serial.println(id);
  p = -1;
  Serial.println("RIPOGGIARE STESSO DITO SUL SENSORE");
  lcd.setCursor(0,2);
  lcd.print("                    ");
  lcd.setCursor(0,2);
  lcd.print("POGGIARE STESSO DITO");
  lcd.setCursor(0,3);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("IMMAGINE ACQUISITA");
      lcd.print("IMPRONTA ACQUISITA");
      delay(1000);
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("ERRORE DI COMUNICAZIONE");
      lcd.print("ERROR COMUNICAZIONE");
      delay(1000);
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("ERRORE DI IMMAGINE");
      lcd.print("ERROR IMMAGINE");
      delay(1000);
      break;
    default:
      Serial.println("ERRORE SCONOSCIUTO");
      lcd.print("ERROR SCONOSCIUTO");
      delay(1000);
      break;
    }
  }
  // OK success!
  p = finger.image2Tz(2);
  lcd.setCursor(0,3);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("IMMAGINE CONVERTITA");
      lcd.print("IMPRONTA CONVERTITA");
      delay(1000);
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("IMMAGINE TROPPO SPORCA");
      lcd.print("IMMAGINE SPORCA");
      delay(1000);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("ERRORE DI COMUNICAZIONE");
      lcd.print("ERROR COMUNICAZIONE");
      delay(1000);
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("IMPOSSIBILE ESTRARRE FEATURE");
      lcd.print("ERROR FEATURE1");
      delay(1000);
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("NON TROVO FEATURE");
      lcd.print("ERROR FEATURE2");
      delay(1000);
      return p;
    default:
      Serial.println("ERRORE SCONOSCIUTO");
      lcd.print("ERROR SCONOSCIUTO");
      delay(1000);
      return p;
  }
  // OK converted!
  lcd.setCursor(0,2);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  lcd.print("                    ");
  lcd.setCursor(0,2);
  Serial.print("CREAZIONE MODELLO PER ID: ");  Serial.println(id);
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("LE IMPRONTE CORRISPONDONO");
    lcd.print("IMPRONTE UGUALI");
    delay(1000);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("ERRORE DI COMUNICAZIONE");
    lcd.print("ERROR COMUNICAZIONE");
    delay(1000);
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("LE IMPRONTE NON CORRISPONDONO");
    lcd.print("IMPRONTE DIVERSE");
    delay(1000);
    return p;
  } else {
    Serial.println("ERRORE SCONOSCIUTO");
    lcd.print("ERRORE SCONOSCIUTO");
    delay(1000);
    return p;
  }
  Serial.print("ID ");
  Serial.println(id);
  lcd.setCursor(0,3);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("IMPRONTA SALVATA!");
    lcd.print("IMPRONTA SALVATA");
    delay(1000);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("ERRORE DI COMUNICAZIONE");
    lcd.print("ERROR COMUNICAZIONE");
    delay(1000);
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("ERRORE DI ALLOCAZIONE DI MEMORIA");
    lcd.print("ERROR MEMORIA");
    delay(1000);
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("ERRORE DI SCRITTURA SU MEMORIA FLASH");
    lcd.print("ERROR FLASH");
    delay(1000);
    return p;
  } else {
    Serial.println("ERRORE SCONOSCIUTO");
    lcd.print("ERRORE SCONOSCIUTO");
    delay(1000);
    return p;
  }
  return true;
}