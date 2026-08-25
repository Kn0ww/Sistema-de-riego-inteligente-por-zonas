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

## Verificación sensor simulado
| Parámetro        | Simulación (GT1) | 
|------------------|------------------|
| m (ganancia)     | 0.94883          |
| b (offset)       | -1.955           |
| Tercer punto     | dentro de tol.   | 
---------------------------------------
- Tolerancia declarada: ±0.5 °C
- Sensor: humedad de suelo capacitivo | Familia: A 
- Referencia: dos condiciones conocidas, aire (0 %) y agua (100 %)
- Referencia validada por: docente de la sección
## Condiciones de la medición 
|             Condición              |               Valor declarado              |
|------------------------------------|--------------------------------------------|
| Tensión de alimentación MEDIDA     |   3,2V (verificado con multímetro)         |
| Canales empleados                  |          Z1: GPIO 32, Z2: GPIO 33          |
| Atenuación del convertidor         | 11 dB, lectura en mV calibrados de fabrica |
| Divisor a la entrada               | NO se emplea (Porque el sensor se alimenta de 3,3V, por lo que la salida se mantiene dentro del rango considerado utilizable por el ADC en esta configuración) |
| Lectura en aire dentro de la zona útil | si |
| Tiempo de estabilización en cada punto |        10 s         |
| Sustrato de cada zona                  | Z1: Agua  Z2: Agua |
| Profundidad de inserción               |     hasta 8cm      |
| Temperatura ambiente                   |        20°C        |
| N de la media móvil                    |          5         |

## Tolerancia declarada
|                      Criterio                         | Tolerancia aceptada |
|-------------------------------------------------------|---------------------|
| Dispersión aceptada en condición estable, por zona    |        ± 2%         |
| Separación mínima exigida entre aire y agua           |        1000mV       |
| Reproducibilidad del punto de agua entre repeticiones |       +/- 60 mV      |

## Calibración de dos puntos
| Zona | mV en aire | mV en agua | Separación (mV) | m (%/mV) | b (%) |
|------|------------|------------|-----------------|----------|-------|
| 1    |    2356.6  | 661.2   |      1695.5        | -0.058980  | 138.9954​ |
| 2    |    2297.5  | 406.6   |      1890.9        | -0.052885  | 121.5045 |

La pendiente m es NEGATIVA en ambas zonas: a mayor humedad, menor lectura.

## Reproducibilidad del punto de agua 
| Zona | Repetición 1 (mV) | Repetición 2 (mV) | Diferencia | Cabe en la tolerancia |
|------|-------------------|-------------------|------------|------------------------|
| 1    | 600               |  725              | 125        | no                     |
| 2    | 632               |  648              | 16         | si                     |

## Verificación en el tercer punto (tierra húmeda)
| Zona | mV  | Porcentaje (%) | Valor SIN recortar | Estable y repetible |
|------|-----|------------|--------------------|---------------------|
| 1    | 1036 | 94.8         | N/A             | si           |
| 2    | 782  | 89.2        | N/A             | si           |

## Dispersión medida 
| Zona | Condición registrada | Media (%) | Dispersión (%) | Banda minima (k x disp) |
|------|----------------------|-----------|----------------|-------------------------|
| 1    | tierra     | 87.44      | 0.28             | 0.84                      |
| 2    | tierra     | 83.2        | 0.20             | 0.60                      |

k declarado: <valor>

## Contraste con la GT1
| Aspecto              | Simulacion (GT1) | Fisico (S4) |
|----------------------|------------------|-------------|
| Origen del error     | sembrado         | propio de cada ejemplar |
| Numero de pares      | 1                | 2, uno por zona |
| Desviacion observada | ---              | <diferencia entre los tres pares> |

### Hallazgo del equipo
<Comparar los dos pares entre si. Indicar cuanto difieren y cual zona resulto la
mas ruidosa, y explicar por que eso obliga a darle una banda de histeresis mayor
que a la otra.>


mucho que los porcentajes resultantes parezcan razonables.
