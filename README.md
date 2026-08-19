# Sistema de riego inteligente por zonas
<img width="1920" height="1048" alt="IoT-e1736873173433" src="https://github.com/user-attachments/assets/297ca89b-2a66-44c3-b533-7e006c271a91" />

## Objetivo
Diseñar e implementar un sistema de riego automático basado en IoT, capaz de monitorear la humedad del suelo y controlar el riego de manera independiente según las necesidades de cada zona.
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
- Mini bomba de agua R385 DC 5V
- Pantalla LCD 16×2 GDM1602K
- Servomotor SG90
- 2 pilas 18650
  
## Software
- Arduino IDE
- Mosquitto
- Telegraf
- InfluxDB
- Grafana
- Python + Pandas
- Node-RED

## Ventana del filtro
- N: 5, Se seleccionó porque para el sistema de riego se mide la humedad del suelo cada 60 segundos. La ventana de cinco muestras permite suavizar variaciones y ruido del sensor considerando aproximadamente cinco minutos de mediciones, sin generar un retraso excesivo para la decisión del riego. 

## Verificación física del sensor (Semana 4, ítem 1 GT2)
Sensor: <HD-38> | Familia: A | Referencia: (cual y quien lo valido)
| Parámetro        | Simulación (GT1) | Físico (S4) | Desviación |
|------------------|------------------|-------------|------------|
| m (ganancia)     | 0.94883          | x           |            |
| b (offset)       | -1.955           | x           |            |
| Tercer punto     | dentro de tol.   | x           |            |
Tolerancia declarada: ±0.5 °C
Condiciones: temperatura , superficie , sustrato u otra según familia.
