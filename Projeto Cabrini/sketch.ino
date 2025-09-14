#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include "DHT.h"
#include <HCSR04.h>

// === DEFINIÇÕES ===
#define DHTPIN 2
#define DHTTYPE DHT22
#define LDR_PIN A0
#define BUZZER_PIN 7
#define LED_ALERT_PIN 3
#define BTN_INC A1
#define BTN_OK A2
#define TRIGGER 5
#define ECHO 6

#define LOG_OPTION 1
#define SERIAL_OPTION 1
#define UTC_OFFSET -3

DHT dht(DHTPIN, DHTTYPE);
RTC_DS1307 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);
UltraSonicDistanceSensor distanceSensor(TRIGGER, ECHO); // HCSR04

// === EEPROM ===
const int maxRecords = 100;
const int recordSize = 10;
int startAddress = 0;
int endAddress = maxRecords * recordSize;
int currentAddress = 0;
int lastLoggedMinute = -1;

// === GATILHOS ===
float trigger_t_min = 15.0;
float trigger_t_max = 25.0;
float trigger_u_min = 30.0;
float trigger_u_max = 50.0;
float trigger_lux_max = 30.0;

// === MENU ===
int menuIndex = 0;
unsigned long lastButtonPress = 0;

// === UNIDADES DE TEMPERATURA ===
int tempUnit = 0;

// === SENSOR DE PROXIMIDADE ===
int limiteProximidadeOn  = 50;  // liga LCD abaixo disso (histerese)
int limiteProximidadeOff = 62;  // desliga LCD acima disso
bool lcdLigado = true;

// === CARACTERES PERSONALIZADOS ===
byte gargaloCheio[] = { B00000, B00000, B00001, B00111, B00111, B00001, B00000, B00000 };
byte corpoGarrafaCheia[] = { B00000, B11111, B11111, B11111, B11111, B11111, B11111, B00000 };
byte fundoGarrafaCheia[] = { B00000, B11110, B11110, B11110, B11110, B11110, B11110, B00000 };
byte fundoGarrafaCheiaEsvaziando[] = { B00000, B11110, B00010, B11110, B11110, B11110, B11110, B00000 };
byte gargalo3[] = { B00000, B00000, B00001, B00111, B00111, B00001, B00000, B00000 };
byte corpoGarrafa3[] = { B00000, B11111, B00000, B11111, B11111, B11111, B11111, B00000 };
byte fundoGarrafa3[] = { B00000, B11110, B00010, B11110, B11110, B11110, B11110, B00000 };
byte fundoGarrafa3Esvaziando[] = { B00000, B11110, B00010, B00010, B11110, B11110, B11110, B00000 };
byte gargalo2[] = { B00000, B00000, B00001, B00110, B00111, B00001, B00000, B00000 };
byte corpoGarrafa2[] = { B00000, B11111, B00000, B00000, B11111, B11111, B11111, B00000 };
byte fundoGarrafa2[] = { B00000, B11110, B00010, B00010, B11110, B11110, B11110, B00000 };
byte fundoGarrafa2Esvaziando[] = { B00000, B11110, B00010, B00010, B00010, B11110, B11110, B00000 };
byte gargalo1[] = { B00000, B00000, B00001, B00110, B00111, B00001, B00000, B00000 };
byte corpoGarrafa1[] = { B00000, B11111, B00000, B00000, B00000, B11111, B11111, B00000 };
byte fundoGarrafa1[] = { B00000, B11110, B00010, B00010, B00010, B11110, B11110, B00000 };
byte fundoGarrafa1Esvaziando[] = { B00000, B11110, B00010, B00010, B00010, B00010, B11110, B00000 };
byte gargaloVazio[] = { B00000, B00000, B00001, B00110, B00110, B00001, B00000, B00000 };
byte corpoGarrafaVazia[] = { B00000, B11111, B00000, B00000, B00000, B00000, B11111, B00000 };
byte fundoGarrafaVazia[] = { B00000, B11110, B00010, B00010, B00010, B00010, B11110, B00000 };
byte TacaVazia[] = { B10001, B10001, B10001, B10001, B01010, B00100, B01110, B11111 };
byte TacaEnchendo[] = { B10001, B10011, B10111, B11111, B01110, B00100, B01110, B11111 };
byte TacaCheia[] = { B10001, B11111, B11111, B11111, B01110, B00100, B01110, B11111 };

void desenhaCena(int bottlePosX, int tacasCheias, int fillingTacaIndex = -1) {
  lcd.clear();
  lcd.setCursor(bottlePosX, 0);
  lcd.write(byte(0));
  lcd.setCursor(bottlePosX + 1, 0);
  lcd.write(byte(1));
  lcd.setCursor(bottlePosX + 2, 0);
  lcd.write(byte(4));
  int posicoes[] = {4, 6, 8, 10};
  for (int i = 0; i < 4; i++) {
    lcd.setCursor(posicoes[i], 1);
    if (i == fillingTacaIndex) {
      lcd.write(byte(5));
    } else if (i < tacasCheias) {
      lcd.write(byte(3));
    } else {
      lcd.write(byte(2));
    }
  }
}

void animarGarrafa() {
  byte* gargalos[] = {gargaloCheio, gargalo3, gargalo2, gargalo1, gargaloVazio};
  byte* corpos[] = {corpoGarrafaCheia, corpoGarrafa3, corpoGarrafa2, corpoGarrafa1, corpoGarrafaVazia};
  byte* fundos[] = {fundoGarrafaCheia, fundoGarrafa3, fundoGarrafa2, fundoGarrafa1, fundoGarrafaVazia};
  byte* fundosEsvaziando[] = {fundoGarrafaCheiaEsvaziando, fundoGarrafa3Esvaziando, fundoGarrafa2Esvaziando, fundoGarrafa1Esvaziando};
  lcd.createChar(2, TacaVazia);
  lcd.createChar(3, TacaCheia);
  lcd.createChar(5, TacaEnchendo);
  int posicoesTacas[] = {4, 6, 8, 10};
  int tempoGotas = 400;
  int tempoEncherRestante = 600;
  int delayMovimento = 1000;
  lcd.createChar(0, gargalos[0]);
  lcd.createChar(1, corpos[0]);
  lcd.createChar(4, fundos[0]);
  desenhaCena(posicoesTacas[0], 0);
  delay(delayMovimento);
  for (int i = 0; i < 4; i++) {
    lcd.createChar(4, fundosEsvaziando[i]);
    desenhaCena(posicoesTacas[i], i, i);
    delay(tempoGotas);
    lcd.createChar(0, gargalos[i + 1]);
    lcd.createChar(1, corpos[i + 1]);
    lcd.createChar(4, fundos[i + 1]);
    desenhaCena(posicoesTacas[i], i + 1);
    delay(tempoEncherRestante);
    if (i < 3) {
      desenhaCena(posicoesTacas[i + 1], i + 1);
      delay(delayMovimento);
    }
  }
  lcd.setCursor(0, 0);
  lcd.print(" CEFSAdega");
  delay(2000);
}

void setup() {
  // (HCSR04 não precisa desses pinModes, mas não atrapalham)
  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT);

  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_ALERT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_INC, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  dht.begin();
  rtc.begin();

  delay(1000);
  lcd.clear();
  animarGarrafa();
}

// --- Função auxiliar: média de 3 leituras válidas ---
float lerDistanciaEstavelCm() {
  float sum = 0.0;
  int cnt = 0;

  for (int i = 0; i < 3; i++) {
    float d = distanceSensor.measureDistanceCm();
    // a lib costuma retornar valor <= 0 quando sem eco / fora do range
    if (d > 0 && d <= 400) { // limite prático do HC-SR04
      sum += d;
      cnt++;
    }
    delay(25); // pequeno intervalo entre pings
  }

  if (cnt == 0) return -1.0;       // nenhuma leitura válida
  return sum / cnt;                // média das válidas
}

void loop() {
  // === LEITURA DO ULTRASSÔNICO (estabilizada) ===
  float distancia = lerDistanciaEstavelCm();
  // === CONTROLE DO LCD COM HISTERSE ===
  if (distancia < 0 || distancia > limiteProximidadeOff) {
    if (lcdLigado) {
      lcd.noBacklight();
      lcd.clear();   // limpa conteúdo ao apagar
      lcdLigado = false;
    }
  } else if (distancia > 0 && distancia < limiteProximidadeOn) {
    if (!lcdLigado) {
      lcd.backlight();
      lcd.clear();   // limpa “fantasma” antes de escrever de novo
      lcdLigado = true;
    }
  }

  // Só atualiza display quando estiver ligado
  if (lcdLigado) {
    // === Sensores principais ===
    DateTime rtcNow = rtc.now();
    int32_t offsetSeconds = UTC_OFFSET * 3600;
    uint32_t ts = rtcNow.unixtime() + offsetSeconds;
    DateTime adjustedTime(ts);

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    int ldrValue = analogRead(LDR_PIN);
    float lightPercent = (float)map(ldrValue, 1023, 0, 0, 100);

    float displayTemp = temperature;
    String unitLabel = "C";
    if (tempUnit == 1) { displayTemp = temperature * 9.0 / 5.0 + 32.0; unitLabel = "F"; }
    else if (tempUnit == 2) { displayTemp = temperature + 273.15; unitLabel = "K"; }

    // === Tela padrão ===
    if (menuIndex == 0) {
      lcd.setCursor(0, 0);
      lcd.print("T:"); lcd.print(displayTemp, 1); lcd.print(unitLabel);
      lcd.print(" U:"); lcd.print(humidity, 1);   lcd.print("%");

      lcd.setCursor(0, 1);
      lcd.print("L:"); lcd.print(lightPercent, 0); lcd.print("% ");
      if (adjustedTime.hour() < 10) lcd.print('0');
      lcd.print(adjustedTime.hour());
      lcd.print(":");
      if (adjustedTime.minute() < 10) lcd.print('0');
      lcd.print(adjustedTime.minute());
    }

    // === Alarmes ===
    bool tempOut  = (temperature < trigger_t_min || temperature > trigger_t_max);
    bool humOut   = (humidity < trigger_u_min    || humidity > trigger_u_max);
    bool lightOut = (lightPercent > trigger_lux_max);

    if (tempOut || humOut || lightOut) {
      digitalWrite(LED_ALERT_PIN, HIGH);
      tone(BUZZER_PIN, 1000, 500);
    } else {
      digitalWrite(LED_ALERT_PIN, LOW);
      noTone(BUZZER_PIN);
    }

    // === Logging (uma vez por minuto quando fora de faixa) ===
    if (adjustedTime.minute() != lastLoggedMinute) {
      lastLoggedMinute = adjustedTime.minute();
      if (tempOut || humOut || lightOut) {
        int tempInt  = (int)(temperature  * 100);
        int humiInt  = (int)(humidity     * 100);
        int lightInt = (int)(lightPercent * 100);
        uint32_t storeTs = ts;

        EEPROM.put(currentAddress, storeTs);
        EEPROM.put(currentAddress + 4, tempInt);
        EEPROM.put(currentAddress + 6, humiInt);
        EEPROM.put(currentAddress + 8, lightInt);
        currentAddress += recordSize;
        if (currentAddress >= endAddress) currentAddress = 0;
      }
    }

    // === Menu quando ligado ===
    handleMenu();
  }

  delay(60); // pequeno respiro do loop
}

// === MENU / UTILITÁRIOS ===
void handleMenu() {
  static bool inLongPress = false;
  if (digitalRead(BTN_OK) == LOW) {
    delay(50);
    if (millis() - lastButtonPress > 3000 && !inLongPress) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Exportando CSV");
      lcd.setCursor(0, 1); lcd.print("via Serial...");
      get_log_csv();
      delay(2000);
      lcd.clear();
      inLongPress = true;
    }
  } else {
    inLongPress = false;
  }

  if (digitalRead(BTN_INC) == LOW) {
    delay(200);
    menuIndex++;
    if (menuIndex > 3) menuIndex = 0;
    lcd.clear();
  }

  if (menuIndex == 3 && digitalRead(BTN_OK) == LOW) {
    delay(200);
    tempUnit++;
    if (tempUnit > 2) tempUnit = 0;
    lcd.clear();
  }

  if (menuIndex == 1) {
    lcd.setCursor(0, 0); lcd.print("Visualizar Ultimo:");
    displayLastLog();
  }
  if (menuIndex == 2) {
    lcd.setCursor(0, 0); lcd.print("Total Registros:");
    lcd.setCursor(0, 1); lcd.print(currentAddress / recordSize);
  }
  if (menuIndex == 3) {
    lcd.setCursor(0, 0); lcd.print("Unidade Temp:");
    lcd.setCursor(0, 1);
    if (tempUnit == 0) lcd.print("Celsius");
    if (tempUnit == 1) lcd.print("Fahrenheit");
    if (tempUnit == 2) lcd.print("Kelvin");
  }
}

void get_log_csv() {
  Serial.println("timestamp,data,hora,temperatura,umidade,luminosidade");
  for (int addr = startAddress; addr < endAddress; addr += recordSize) {
    uint32_t timestamp;
    int tempInt, humiInt, lightInt;
    EEPROM.get(addr, timestamp);
    EEPROM.get(addr + 4, tempInt);
    EEPROM.get(addr + 6, humiInt);
    EEPROM.get(addr + 8, lightInt);

    if (timestamp != 0xFFFFFFFF) {
      DateTime dt((uint32_t)timestamp);
      float t = tempInt / 100.0;
      float h = humiInt / 100.0;
      float l = lightInt / 100.0;

      Serial.print(timestamp); Serial.print(",");
      Serial.print(dt.day());  Serial.print("/");
      Serial.print(dt.month());Serial.print("/");
      Serial.print(dt.year()); Serial.print(",");
      if (dt.hour() < 10) Serial.print('0');
      Serial.print(dt.hour()); Serial.print(":");
      if (dt.minute() < 10) Serial.print('0');
      Serial.print(dt.minute()); Serial.print(":");
      if (dt.second() < 10) Serial.print('0');
      Serial.print(dt.second()); Serial.print(",");
      Serial.print(t); Serial.print(",");
      Serial.print(h); Serial.print(",");
      Serial.println(l);
    }
  }
}

void displayLastLog() {
  int lastAddr = currentAddress - recordSize;
  if (lastAddr < 0) lastAddr = endAddress - recordSize;

  uint32_t timestamp;
  int tempInt, humiInt, lightInt;
  EEPROM.get(lastAddr, timestamp);
  EEPROM.get(lastAddr + 4, tempInt);
  EEPROM.get(lastAddr + 6, humiInt);
  EEPROM.get(lastAddr + 8, lightInt);

  lcd.setCursor(0, 1);
  lcd.print(tempInt / 100.0); lcd.print("C ");
  lcd.print(humiInt / 100.0); lcd.print("% ");
}
