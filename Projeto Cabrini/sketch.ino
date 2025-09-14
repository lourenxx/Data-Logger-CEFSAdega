// Data_Logger_EventDriven.ino
// Registro orientado a EVENTO: grava quando qualquer parâmetro ultrapassa o limite
// Mantém o mesmo layout de registro na EEPROM (10 bytes):
// [0..3]  uint32_t unixTs, [4..5] int16 temp*100, [6..7] int16 umid*100, [8..9] int16 luz*100

#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include "DHT.h"
#include <HCSR04.h>


// === DEFINIÇÕES DE HARDWARE ===
#define DHTPIN 2
#define DHTTYPE DHT22
#define LDR_PIN A0
#define BUZZER_PIN 7
#define LED_R_PIN 9
#define LED_G_PIN 10
#define LED_B_PIN 11
#define BTN_INC A1
#define BTN_OK A2
#define BTN_UNIT A3 // Botão para alternar unidade
#define TRIGGER 6
#define ECHO 5

// === OPCOES ===
#define LOG_OPTION 1
#define SERIAL_OPTION 1
#define UTC_OFFSET -3 // Horário local = UTC + offset (em horas)

DHT dht(DHTPIN, DHTTYPE);
RTC_DS1307 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);
UltraSonicDistanceSensor distanceSensor(TRIGGER, ECHO); // HCSR04


// === EEPROM / REGISTROS ===
const int maxRecords = 100;
const int recordSize = 10; // 4 + 2 + 2 + 2
const int startAddress = 0;
const int endAddress = maxRecords * recordSize; // 1000 bytes
int currentAddress = 0;                         // ponteiro circular

const int tempUnitEEPROMAddr = 300; // Endereço para salvar unidade (1 byte)

// === LIMITES (GATILHOS) ===
float trigger_t_min = 15.0;
float trigger_t_max = 25.0;
float trigger_u_min = 30.0;
float trigger_u_max = 50.0;
float trigger_lux_max = 30.0; // apenas limite superior

// === MENU / ESTADO ===
int menuIndex = 0;
unsigned long lastButtonPress = 0; // mantido conforme código original
int tempUnit = 0;                  // 0=C, 1=F, 2=K

// === ESTADO PARA DETECÇÃO DE BORDAS ===
bool prevTempOut = false;
bool prevHumOut = false;
bool prevLightOut = false;
bool statesInitialized = false;

// === SENSOR DE PROXIMIDADE ===
int limiteProximidadeOn = 50;  // liga LCD abaixo disso (histerese)
int limiteProximidadeOff = 62; // desliga LCD acima disso
bool lcdLigado = true;

// === CARACTERES PERSONALIZADOS ===
byte gargaloCheio[] = {B00000, B00000, B00001, B00111, B00111, B00001, B00000, B00000};
byte corpoGarrafaCheia[] = {B00000, B11111, B11111, B11111, B11111, B11111, B11111, B00000};
byte fundoGarrafaCheia[] = {B00000, B11110, B11110, B11110, B11110, B11110, B11110, B00000};
byte fundoGarrafaCheiaEsvaziando[] = {B00000, B11110, B00010, B11110, B11110, B11110, B11110, B00000};
byte gargalo3[] = {B00000, B00000, B00001, B00111, B00111, B00001, B00000, B00000};
byte corpoGarrafa3[] = {B00000, B11111, B00000, B11111, B11111, B11111, B11111, B00000};
byte fundoGarrafa3[] = {B00000, B11110, B00010, B11110, B11110, B11110, B11110, B00000};
byte fundoGarrafa3Esvaziando[] = {B00000, B11110, B00010, B00010, B11110, B11110, B11110, B00000};
byte gargalo2[] = {B00000, B00000, B00001, B00110, B00111, B00001, B00000, B00000};
byte corpoGarrafa2[] = {B00000, B11111, B00000, B00000, B11111, B11111, B11111, B00000};
byte fundoGarrafa2[] = {B00000, B11110, B00010, B00010, B11110, B11110, B11110, B00000};
byte fundoGarrafa2Esvaziando[] = {B00000, B11110, B00010, B00010, B00010, B11110, B11110, B00000};
byte gargalo1[] = {B00000, B00000, B00001, B00110, B00111, B00001, B00000, B00000};
byte corpoGarrafa1[] = {B00000, B11111, B00000, B00000, B00000, B11111, B11111, B00000};
byte fundoGarrafa1[] = {B00000, B11110, B00010, B00010, B00010, B11110, B11110, B00000};
byte fundoGarrafa1Esvaziando[] = {B00000, B11110, B00010, B00010, B00010, B00010, B11110, B00000};
byte gargaloVazio[] = {B00000, B00000, B00001, B00110, B00110, B00001, B00000, B00000};
byte corpoGarrafaVazia[] = {B00000, B11111, B00000, B00000, B00000, B00000, B11111, B00000};
byte fundoGarrafaVazia[] = {B00000, B11110, B00010, B00010, B00010, B00010, B11110, B00000};
byte TacaVazia[] = {B10001, B10001, B10001, B10001, B01010, B00100, B01110, B11111};
byte TacaEnchendo[] = {B10001, B10011, B10111, B11111, B01110, B00100, B01110, B11111};
byte TacaCheia[] = {B10001, B11111, B11111, B11111, B01110, B00100, B01110, B11111};

void desenhaCena(int bottlePosX, int tacasCheias, int fillingTacaIndex = -1)
{
  lcd.clear();
  lcd.setCursor(bottlePosX, 0);
  lcd.write(byte(0));
  lcd.setCursor(bottlePosX + 1, 0);
  lcd.write(byte(1));
  lcd.setCursor(bottlePosX + 2, 0);
  lcd.write(byte(4));

  int posicoes[] = {4, 6, 8, 10};
  for (int i = 0; i < 4; i++)
  {
    lcd.setCursor(posicoes[i], 1);
    if (i == fillingTacaIndex)
    {
      lcd.write(byte(5));
    }
    else if (i < tacasCheias)
    {
      lcd.write(byte(3));
    }
    else
    {
      lcd.write(byte(2));
    }
  }
}

void animarGarrafa()
{
  byte *gargalos[] = {gargaloCheio, gargalo3, gargalo2, gargalo1, gargaloVazio};
  byte *corpos[] = {corpoGarrafaCheia, corpoGarrafa3, corpoGarrafa2, corpoGarrafa1, corpoGarrafaVazia};
  byte *fundos[] = {fundoGarrafaCheia, fundoGarrafa3, fundoGarrafa2, fundoGarrafa1, fundoGarrafaVazia};
  byte *fundosEsvaziando[] = {fundoGarrafaCheiaEsvaziando, fundoGarrafa3Esvaziando, fundoGarrafa2Esvaziando, fundoGarrafa1Esvaziando};

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

  for (int i = 0; i < 4; i++)
  {
    lcd.createChar(4, fundosEsvaziando[i]);
    desenhaCena(posicoesTacas[i], i, i);
    delay(tempoGotas);

    lcd.createChar(0, gargalos[i + 1]);
    lcd.createChar(1, corpos[i + 1]);
    lcd.createChar(4, fundos[i + 1]);
    desenhaCena(posicoesTacas[i], i + 1);
    delay(tempoEncherRestante);

    if (i < 3)
    {
      desenhaCena(posicoesTacas[i + 1], i + 1);
      delay(delayMovimento);
    }
  }

  lcd.setCursor(0, 0);
  lcd.print(" CEFSAdega");
  delay(2000);
}

void checkTempUnitButton()
{
  static unsigned long lastPress = 0;
  if (digitalRead(BTN_UNIT) == LOW)
  {
    if (millis() - lastPress > 500)
    {
      tempUnit++;
      if (tempUnit > 2)
        tempUnit = 0;
      EEPROM.update(tempUnitEEPROMAddr, (uint8_t)tempUnit);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Unidade Temp:");
      lcd.setCursor(0, 1);
      if (tempUnit == 0)
        lcd.print("Celsius");
      if (tempUnit == 1)
        lcd.print("Fahrenheit");
      if (tempUnit == 2)
        lcd.print("Kelvin");
      delay(1000);
    }
    lastPress = millis();
  }
}

void logRecord(uint32_t unixTs, float temperature, float humidity, float lightPercent)
{
  int16_t tempInt = (int16_t)(temperature * 100);
  int16_t humiInt = (int16_t)(humidity * 100);
  int16_t lightInt = (int16_t)(lightPercent * 100);

  EEPROM.put(currentAddress, unixTs);
  EEPROM.put(currentAddress + 4, tempInt);
  EEPROM.put(currentAddress + 6, humiInt);
  EEPROM.put(currentAddress + 8, lightInt);
  getNextAddress();
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_INC, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  pinMode(BTN_UNIT, INPUT_PULLUP);

  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  dht.begin();
  rtc.begin();

  delay(1000);
  lcd.clear();

  tempUnit = EEPROM.read(tempUnitEEPROMAddr);
  if (tempUnit > 2)
    tempUnit = 0;

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


void loop()
{

  // === LEITURA DO ULTRASSÔNICO (estabilizada) ===

  float distancia = lerDistanciaEstavelCm();

  if (distancia < 0 || distancia > limiteProximidadeOff)
  {

    if (lcdLigado)
    {

      lcd.noBacklight();

      lcd.clear(); // limpa conteúdo ao apagar

      lcdLigado = false;
    }
  }
  else if (distancia > 0 && distancia < limiteProximidadeOn)
  {

    if (!lcdLigado)
    {

      lcd.backlight();

      lcd.clear(); // limpa “fantasma” antes de escrever de novo

      lcdLigado = true;
    }
  }

  if (lcdLigado)
  {
    checkTempUnitButton();
    handleMenu();

    // Tempo local (UTC + offset)
    DateTime utcNow = rtc.now();
    DateTime localNow = utcNow + TimeSpan(UTC_OFFSET * 3600);

    // Leituras de sensores
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature(); // Celsius
    int ldrValue = analogRead(LDR_PIN);
    float lightPercent = map(ldrValue, 1023, 0, 0, 100);

    // Conversão para exibição
    float displayTemp = temperature;
    String unitLabel = "C";
    if (tempUnit == 1)
    {
      displayTemp = temperature * 9.0 / 5.0 + 32.0;
      unitLabel = "F";
    }
    else if (tempUnit == 2)
    {
      displayTemp = temperature + 273.15;
      unitLabel = "K";
    }

    // Tela principal
    if (menuIndex == 0)
    {
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(displayTemp, 1);
      lcd.print(unitLabel);
      lcd.print(" U:");
      lcd.print(humidity, 1);
      lcd.print("%");

      lcd.setCursor(0, 1);
      lcd.print("L:");
      lcd.print(lightPercent, 0);
      lcd.print("% ");
      lcd.print(localNow.hour());
      lcd.print(":");
      lcd.print(localNow.minute());
    }

    // Checagem de limites (estado atual)
    bool tempOut = (temperature < trigger_t_min) || (temperature > trigger_t_max);
    bool humOut = (humidity < trigger_u_min) || (humidity > trigger_u_max);
    bool lightOut = (lightPercent > trigger_lux_max);

    setRGBColor(tempOut, humOut, lightOut, temperature, humidity, lightPercent);

    // Alerta sonoro/visual (mantido)
    if (tempOut || humOut || lightOut)
    {
      tone(BUZZER_PIN, 1000, 500);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("! ALERTA !");
      lcd.setCursor(0, 1);
      if (tempOut)
        lcd.print("Temp fora limite");
      else if (humOut)
        lcd.print("Umid fora limite");
      else if (lightOut)
        lcd.print("Luz fora limite");
      delay(1500);
    }

    // ===== REGISTRO ORIENTADO A EVENTO =====
    if (!statesInitialized)
    {
      prevTempOut = tempOut;
      prevHumOut = humOut;
      prevLightOut = lightOut;
      statesInitialized = true;
    }

    bool crossedTemp = tempOut && !prevTempOut;
    bool crossedHum = humOut && !prevHumOut;
    bool crossedLight = lightOut && !prevLightOut;

    if (crossedTemp || crossedHum || crossedLight)
    {
      logRecord(localNow.unixtime(), temperature, humidity, lightPercent);
    }

    prevTempOut = tempOut;
    prevHumOut = humOut;
    prevLightOut = lightOut;
    // =======================================

    delay(500);
  }
}

void handleMenu()
{
  static bool inLongPress = false;

  if (digitalRead(BTN_OK) == LOW)
  {
    delay(50);
    if (millis() - lastButtonPress > 3000 && !inLongPress)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Exportando CSV");
      lcd.setCursor(0, 1);
      lcd.print("via Serial...");
      get_log_csv();
      delay(2000);
      lcd.clear();
      inLongPress = true;
    }
  }
  else
  {
    inLongPress = false;
    lastButtonPress = millis(); // mantém lógica original
  }

  if (digitalRead(BTN_INC) == LOW)
  {
    delay(200);
    menuIndex++;
    if (menuIndex > 3)
      menuIndex = 0;
    lcd.clear();
  }

  if (menuIndex == 1)
  {
    lcd.setCursor(0, 0);
    lcd.print("Visualizar Ultimo:");
    displayLastLog();
  }
  if (menuIndex == 2)
  {
    lcd.setCursor(0, 0);
    lcd.print("Total Registros:");
    lcd.setCursor(0, 1);
    lcd.print(currentAddress / recordSize);
  }
  if (menuIndex == 3)
  {
    lcd.setCursor(0, 0);
    lcd.print("Unidade Temp:");
    lcd.setCursor(0, 1);
    if (tempUnit == 0)
      lcd.print("Celsius");
    if (tempUnit == 1)
      lcd.print("Fahrenheit");
    if (tempUnit == 2)
      lcd.print("Kelvin");
  }
}

void getNextAddress()
{
  currentAddress += recordSize;
  if (currentAddress >= endAddress)
  {
    currentAddress = 0; // ring buffer
  }
}

void get_log_csv()
{
  Serial.println("timestamp,data,hora,temperatura,umidade,luminosidade");
  for (int addr = startAddress; addr < endAddress; addr += recordSize)
  {
    uint32_t timestamp = 0xFFFFFFFF;
    int16_t tempInt = 0, humiInt = 0, lightInt = 0;

    EEPROM.get(addr, timestamp);
    EEPROM.get(addr + 4, tempInt);
    EEPROM.get(addr + 6, humiInt);
    EEPROM.get(addr + 8, lightInt);

    if (timestamp != 0xFFFFFFFF)
    {
      DateTime dt = DateTime(timestamp);
      float t = tempInt / 100.0;
      float h = humiInt / 100.0;
      float l = lightInt / 100.0;

      Serial.print(timestamp);
      Serial.print(",");
      Serial.print(dt.day());
      Serial.print("/");
      Serial.print(dt.month());
      Serial.print("/");
      Serial.print(dt.year());
      Serial.print(",");
      Serial.print(dt.hour());
      Serial.print(":");
      Serial.print(dt.minute());
      Serial.print(":");
      Serial.print(dt.second());
      Serial.print(",");
      Serial.print(t);
      Serial.print(",");
      Serial.print(h);
      Serial.print(",");
      Serial.println(l);
    }
  }
}

void setRGBColor(bool tempOut, bool humOut, bool lightOut,
                 float temperature, float humidity, float lightPercent)
{
  // Margens desejadas: 2 °C para T e 5 p.p. para U e Luz (near-limit)
  const float MARGEM_T = 2.0; // graus Celsius
  const float MARGEM_P = 5.0; // pontos percentuais (%RH e % de luz)

  bool anyOut = (tempOut || humOut || lightOut);

  bool nearTemp = (!tempOut) && ((temperature >= (trigger_t_min) && (temperature - trigger_t_min) <= MARGEM_T) ||
                                 (temperature <= (trigger_t_max) && (trigger_t_max - temperature) <= MARGEM_T));
  bool nearHum = (!humOut) && ((humidity >= (trigger_u_min) && (humidity - trigger_u_min) <= MARGEM_P) ||
                               (humidity <= (trigger_u_max) && (trigger_u_max - humidity) <= MARGEM_P));
  bool nearLux = (!lightOut) && ((lightPercent <= trigger_lux_max) && ((trigger_lux_max - lightPercent) <= MARGEM_P));

  bool nearLimit = (nearTemp || nearHum || nearLux);

  if (anyOut)
  {
    // Vermelho
    analogWrite(LED_R_PIN, 255);
    analogWrite(LED_G_PIN, 0);
    analogWrite(LED_B_PIN, 0);
  }
  else if (nearLimit)
  {
    // Amarelo
    analogWrite(LED_R_PIN, 255);
    analogWrite(LED_G_PIN, 255);
    analogWrite(LED_B_PIN, 0);
  }
  else
  {
    // Verde
    analogWrite(LED_R_PIN, 0);
    analogWrite(LED_G_PIN, 255);
    analogWrite(LED_B_PIN, 0);
  }
}

void displayLastLog()
{
  int lastAddr = currentAddress - recordSize;
  if (lastAddr < 0)
    lastAddr = endAddress - recordSize;

  uint32_t timestamp = 0xFFFFFFFF;
  int16_t tempInt = 0, humiInt = 0, lightInt = 0;

  EEPROM.get(lastAddr, timestamp);
  EEPROM.get(lastAddr + 4, tempInt);
  EEPROM.get(lastAddr + 6, humiInt);
  EEPROM.get(lastAddr + 8, lightInt);

  float t = tempInt / 100.0;
  if (tempUnit == 1)
    t = t * 9.0 / 5.0 + 32.0;
  if (tempUnit == 2)
    t = t + 273.15;

  lcd.setCursor(0, 1);
  lcd.print(t);
  if (tempUnit == 0)
    lcd.print("C ");
  if (tempUnit == 1)
    lcd.print("F ");
  if (tempUnit == 2)
    lcd.print("K ");
  lcd.print(humiInt / 100.0);
  lcd.print("% ");
}