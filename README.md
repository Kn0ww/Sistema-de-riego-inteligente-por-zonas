# Sistema de riego inteligente por zonas
<img width="1920" height="1048" alt="IoT-e1736873173433" src="https://github.com/user-attachments/assets/297ca89b-2a66-44c3-b533-7e006c271a91" />

## Objetivo
Diseñar e implementar un sistema de riego automático basado en IoT, capaz de monitorear la humedad del suelo y controlar el riego de manera independiente para 2 ZONAS validada por el docente de la sección.

## Integrantes
- David Morales  
- Alex Camacaro
- Sofia Aedo
- Mauricio Lobo
- Xavier Lopez
  
## Hardware
- Microcontrolador ESP32
- 2 sensores de humedad HD-38
- Relé de 4 canales HW-316
- Mini bomba de agua R385 DC 5V
- Pantalla LCD 16×2 GDM1602K
- Servomotor SG90
  
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
- Tolerancia declarada sensor simulado: ±0.5 °C

## Sistema de riego

- Sensor: humedad de suelo resistivo | Familia: A 
- Referencia: dos condiciones conocidas, aire (0 %) y agua (100 %)
- Porcentaje humedad sensores(%): El sensor entrega un índice relativo de humedad para nuestro sustrato.
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
| 1    | 719 | 96.6         | N/A             | si           |
| 2    | 782  | 89.2        | N/A             | si           |

## Dispersión medida 
| Zona | Condición registrada | Media (%) | Dispersión (%) | Banda minima (k x disp) |
|------|----------------------|-----------|----------------|-------------------------|
| 1    | tierra     | 87.44      | 0.28             | 0.84                      |
| 2    | tierra     | 83.2        | 0.20             | 0.60                      |

k declarado: 3

## Contraste con la GT1
| Aspecto              | Simulacion (GT1) | Fisico (S4) |
|----------------------|------------------|-------------|
| Origen del error     | sembrado         | propio de cada ejemplar |
| Numero de pares      | 1                | 2, uno por zona |
| Desviacion observada | ---              | <diferencia entre los tres pares> |

## Hallazgo del equipo
La diferencia entre las medias es de 4,24 puntos porcentuales, mientras que la dispersión de la Zona 1 es 0,08 puntos porcentuales mayor que la de la Zona 2. Por lo tanto, la Zona 1 resultó ser la más dispersa.

Esto obliga a darle a la Zona 1 una banda de histéresis mayor, ya que presenta más variación en sus lecturas. Una banda demasiado pequeña podría hacer que el sistema cambie repetidamente entre regar y no regar debido al ruido del sensor, provocando una oscilación de la bomba. La Zona 2, al ser menos dispersa, puede utilizar una banda menor.

### Limitaciones registradas
- La escala construida vale para el ejemplar, el sustrato Y la profundidad de
  insercion declarados. Cambiar cualquiera de los tres obliga a recalibrar.
- La lectura se toma en milivolts calibrados de fabrica. El convertidor del
  ESP32 responde de forma util entre unos 150 y 2450 mV: fuera de ese rango la
  medicion se comprime o se recorta.
- El porcentaje informado es una posicion relativa entre aire y agua, no un
  contenido volumetrico de agua medido contra patron.
- La dispersion registrada corresponde a la condicion declarada. En otra
  condicion, la dispersion puede ser distinta.
- P8 es un prototipo educativo.
