# Sistema de riego inteligente por zonas
<img width="1920" height="1048" alt="IoT-e1736873173433" src="https://github.com/user-attachments/assets/297ca89b-2a66-44c3-b533-7e006c271a91" />

## Objetivo
Diseñar un sistema de IoT de riego automático basado en la humedad del suelo por zonas.

## Integrantes
- David Morales
- Alex Camacaro
- Sofia Aedo
- Mauricio Lobo
- Xavier Lopez
  
## Hardware
- Microcontrolador ESP32
- 2 Sensor de humedad de suelo HD-38
- Relé de 4 canales HW-316
- Mini bomba de agua R385 DC 3–6V
  
## Software
- Arduino IDE
- Mosquitto
- Telegraf
- InfluxDB
- Grafana
- Python + Pandas
- Node-RED

## Calibración
- m: 0.94883
- b: -1.955 °C
- Tolerancia declarada: ±0.5 °C.
