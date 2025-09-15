# 📊 Data Logger Orientado a Eventos com Arduino

![Arduino](https://img.shields.io/badge/Arduino-Uno-blue?logo=arduino)  
![C++](https://img.shields.io/badge/Language-C%2B%2B-green)  
![EEPROM](https://img.shields.io/badge/EEPROM-logging-orange)  
![License](https://img.shields.io/badge/license-MIT-lightgrey)

## 🔎 Visão Geral
Este projeto implementa um **Data Logger orientado a eventos** utilizando Arduino.  
Ele monitora **temperatura, umidade, luminosidade e proximidade**, armazenando dados na **EEPROM** somente quando algum parâmetro ultrapassa os limites definidos.  

Dessa forma, a memória é usada de forma eficiente e os eventos críticos são preservados para análise posterior.  
A interface é feita em um **LCD 16x2 I2C** com menu de navegação, botões físicos e alertas visuais/sonoros.

---

## ⚡ Funcionalidades

- ✅ **Registro orientado a evento**: só grava quando ultrapassa limites.  
- ✅ **Sensores múltiplos**:  
  - DHT22 (temperatura e umidade)  
  - LDR (luminosidade)  
  - HC-SR04 (proximidade → ativa/desativa LCD)  
- ✅ **Armazenamento em EEPROM**: buffer circular de até **100 registros (10 bytes cada)**.  
- ✅ **LED RGB de status**:  
  - Verde → normal  
  - Amarelo → próximo ao limite  
  - Vermelho → fora do limite  
- ✅ **Alerta sonoro (buzzer)** quando algum parâmetro sai da faixa.  
- ✅ **Exportação de logs em CSV via Serial**.  
- ✅ **Menu interativo no LCD** com:  
  - Último registro  
  - Total de registros  
  - Unidade de temperatura (°C / °F / K)

---

## 🔧 Hardware Utilizado

| Componente | Função |
|------------|--------|
| Arduino UNO/Nano | Microcontrolador |
| DHT22 | Sensor de temperatura e umidade |
| LDR + resistor | Medição de luminosidade |
| HC-SR04 | Sensor ultrassônico de proximidade |
| LCD 16x2 I2C | Interface de exibição |
| LEDs RGB | Indicadores visuais de status |
| Buzzer | Alerta sonoro |
| Botões (INC, OK, UNIT) | Navegação de menu e configurações |
| EEPROM interna | Armazenamento de registros |

---

## 📂 Estrutura de Dados na EEPROM

Cada registro ocupa **10 bytes**, no formato:

| Byte(s) | Tipo     | Conteúdo                   |
|---------|----------|----------------------------|
| 0–3     | `uint32` | Timestamp (Unix)           |
| 4–5     | `int16`  | Temperatura ×100           |
| 6–7     | `int16`  | Umidade ×100               |
| 8–9     | `int16`  | Luminosidade ×100          |

- Total de registros: **100** → 1000 bytes  
- Estratégia: **buffer circular** (sobrescreve os mais antigos quando cheio)

---

## ▶️ Como Usar

1. **Bibliotecas necessárias** (instale na Arduino IDE):  
   - `Wire.h`  
   - `EEPROM.h`  
   - `LiquidCrystal_I2C.h`  
   - `RTClib.h`  
   - `DHT.h`  
   - `HCSR04.h`  

2. **Carregue o código** no Arduino pela IDE.  

3. **Conexões principais**:
   - DHT22 → pino `D2`  
   - LDR → pino `A0`  
   - HC-SR04 → `TRIGGER=6`, `ECHO=5`  
   - LCD → interface I2C (`0x27`)  
   - Buzzer → pino `D7`  
   - LED RGB → `R=9`, `G=10`, `B=11`  
   - Botões: `INC=A1`, `OK=A2`, `UNIT=A3`

4. **Uso**:
   - O LCD liga apenas quando há proximidade detectada.  
   - O sistema monitora sensores em tempo real.  
   - Ao ultrapassar limites → evento é salvo na EEPROM.  
   - Segure o botão **OK** por 3s → exporta todos os registros em **CSV via Serial**.  

---

## 📊 Exemplo de Exportação (Serial/CSV)

```csv
timestamp,data,hora,temperatura,umidade,luminosidade
1736943382,14/09/2025,18:23:02,24.53,45.12,12.4
1736943521,14/09/2025,18:25:21,26.10,52.30,35.0
```

---

## 🚀 Melhorias Futuras

- Gravação em **cartão SD** para expandir capacidade.  
- Envio de dados em **tempo real via Wi-Fi (ESP8266/ESP32)**.  
- Interface Web para consulta dos registros.  
- Ajuste dinâmico de limites via menu.  

---

📌 **Autor:** Projeto acadêmico/experimental desenvolvido no **CEFSAdega** 🍷  
