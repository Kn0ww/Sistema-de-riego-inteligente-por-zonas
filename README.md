# E03_P8-Sistema-de-riego-inteligente-por-zonas
Proyecto de automatización de riego utilizando ESP32 y sensores.

## Objetivo
Crear un sistema capaz de controlar una bomba de agua automáticamente
según la humedad del suelo.

## Hardware
- Microcontrolador ESP32
- Sensor de humedad de suelo MCI07637
- Relé de 4 canales MCI02798
- Micro bomba de agua 
  
## Software
- Arduino IDE / PlatformIO

## Funcionamiento
El ESP32 lee los sensores y activa la bomba cuando la humedad baja
del nivel configurado.
