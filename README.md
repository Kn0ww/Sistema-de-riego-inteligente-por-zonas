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

## Verificación fisica del sensor

- Sensor: humedad de suelo capacitivo | Familia: A 
- Referencia: dos condiciones conocidas, aire (0 %) y agua (100 %)
- Referencia validada por: docente de la seccion

### Condiciones de la medición
| Condición                          | Valor declarado |
|------------------------------------|-----------------|
| Tensión de alimentación medida     | nominal 3,3 V |
| Canales empleados                  | Z1: GPIO 32,  Z2: GPIO 33 |
| Atenuación del convertidor         | 11 dB |
| Divisor a la entrada               | NO se emplea (justificacion en la hoja de conexion,), porque el sensor se alimenta de 3,3 V, por lo que la salida se mantiene dentro del rango considerado utilizable por el ADC en esta configuración |
| Sustrato de cada zona              | Z1: agua  Z2: agua |
| Profundidad de inserción           | La profundidad fue 8 cm |
| Temperatura ambiente               | La temperatura ambiente es de 20°C |
| N de la media móvil                | 5 |

### Tolerancia declarada
| Criterio                                          | Tolerancia aceptada |
|---------------------------------------------------|---------------------|
| Dispersión aceptada en condición estable, por zona | ± 2%|
| Separación mínima exigida entre aire y agua        | 500 mV |


## Verificación física del sensor (Semana 4, ítem 1 GT2)
Sensor: Humedad HD-38 | Familia: A | Referencia: docente de la seccion
| Parámetro        | Simulación (GT1) | Físico (S4) | Desviación |
|------------------|------------------|-------------|------------|
| m (ganancia)     | 0.94883          | x           |            |
| b (offset)       | -1.955           | x           |            |
| Tercer punto     | dentro de tol.   | x           |            |
------------------------------------------------------------------
Tolerancia declarada: ±0.5 °C
Condiciones: temperatura , superficie , sustrato u otra según familia.
