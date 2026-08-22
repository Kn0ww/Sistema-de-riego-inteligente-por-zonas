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
Sensor: Humedad HD-38 | Familia: A | Referencia: docente de la sección
| Parámetro        | Simulación (GT1) | Físico (S4) | Desviación |
|------------------|------------------|-------------|------------|
| m (ganancia)     | 0.94883          | x           |            |
| b (offset)       | -1.955           | x           |            |
| Tercer punto     | dentro de tol.   | x           |            |
------------------------------------------------------------------
- Tolerancia declarada: ±0.5 °C

- Sensor: humedad de suelo capacitivo | Familia: A 
- Referencia: dos condiciones conocidas, aire (0 %) y agua (100 %)
- Referencia validada por: docente de la sección

### Condiciones de la medición (se fijan ANTES de medir)
|             Condición              |               Valor declarado              |
|------------------------------------|--------------------------------------------|
| Tensión de alimentación MEDIDA     |   3,2V (verificado con multímetro)         |
| Canales empleados                  |          Z1: GPIO 32, Z2: GPIO 33          |
| Atenuación del convertidor         | 11 dB, lectura en mV calibrados de fabrica |
| Divisor a la entrada               | NO se emplea (Porque el sensor se alimenta de 3,3V, por lo que la salida se mantiene dentro del rango considerado utilizable por el ADC en esta configuración) |
| Lectura en aire dentro de la zona útil | no (si toco el tope, NO se calibro |
| Tiempo de estabilización en cada punto |        7 s         |
| Sustrato de cada zona                  | Z1: Agua  Z2: Agua |
| Profundidad de inserción               |     hasta 8cm      |
| Temperatura ambiente                   |        20°C        |
| N de la media móvil                    |          5         |

### Tolerancia declarada
|                      Criterio                         | Tolerancia aceptada |
|-------------------------------------------------------|---------------------|
| Dispersión aceptada en condición estable, por zona    |        ± 2%         |
| Separación mínima exigida entre aire y agua           |        500mV        |
| Reproducibilidad del punto de agua entre repeticiones |       <+/- mV>      |

### Calibración de dos puntos
| Zona | mV en aire | mV en agua | Separación (mV) | m (%/mV) | b (%) |
|------|------------|------------|-----------------|----------|-------|
| 1    |    3124    | 922.5   |      2201.5        | −0.0454236 | 141.9032​ |
| 2    |    3124    |          |               | <>       | <>    |

La pendiente m es NEGATIVA en ambas zonas: a mayor humedad, menor lectura.

Reproducibilidad del punto de agua (se repite al menos dos veces por zona):
| Zona | Repeticion 1 (mV) | Repeticion 2 (mV) | Diferencia | Cabe en la tolerancia |
|------|-------------------|-------------------|------------|------------------------|
| 1    | <>                | <>                | <>         | si / no                |
| 2    | <>                | <>                | <>         | si / no                |

Si el punto de agua no reproduce, la condicion no esta controlada: casi siempre
es la profundidad de insercion. No se elige la repeticion que da el resultado
mas bonito; se controla la condicion y se repiten ambas.

### Verificacion en el tercer punto (tierra humeda)
| Zona | mV  | Porcentaje | Valor SIN recortar | Estable y repetible |
|------|-----|------------|--------------------|---------------------|
| 1    | <>  | <>         | <>                 | si / no             |
| 2    | <>  | <>         | <>                 | si / no             |

El valor sin recortar importa: un 100 % en pantalla no distingue entre llegar
justo y pasarse. Si al sumergir el valor real supera 100, la lectura de hoy es
menor que la del dia de la calibracion y la referencia no reproduce.

No existe patron de humedad en el laboratorio: la verificacion es de COHERENCIA
(0 % al aire, 100 % en agua, valor intermedio estable en tierra humeda), no de
exactitud contra un instrumento de referencia.

### Dispersion medida (paso 4 + script)
| Zona | Condicion registrada | Media (%) | Dispersion (%) | Banda minima (k x disp) |
|------|----------------------|-----------|----------------|-------------------------|
| 1    | <aire / tierra>      | <>        | <>             | <>                      |
| 2    | <>                   | <>        | <>             | <>                      |

k declarado: <valor>

### Contraste con la GT1 (simulacion)
En la GT1 el equipo calibro un sensor analogico simulado, donde el par (m, b)
corregia un error sembrado por software. Aqui la cadena es la misma —cuentas del
ADC1, dos puntos, filtro— pero el error ya existe y no se inyecta, y el modelo
no se corrige: se construye, porque el sensor no tiene funcion de transferencia
publicada.

| Aspecto              | Simulacion (GT1) | Fisico (S4) |
|----------------------|------------------|-------------|
| Origen del error     | sembrado         | propio de cada ejemplar |
| Numero de pares      | 1                | 2, uno por zona |
| Desviacion observada | ---              | <diferencia entre los tres pares> |

### Hallazgo del equipo
<Comparar los dos pares entre si. Indicar cuanto difieren y cual zona resulto la
mas ruidosa, y explicar por que eso obliga a darle una banda de histeresis mayor
que a la otra.>

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
```
---
Lo que revisa el docente en el ítem 1 (familia A, dos unidades)
¿Hay dos pares (m, b), rotulados por zona, y no uno copiado dos veces?
¿Ninguna lectura de calibración tocó el tope del convertidor, y está la tensión de alimentación medida?
¿Está la referencia —aire y agua— y quién la validó?
¿Está el resultado físico junto al de simulación de la GT1, con la desviación?
¿Las tolerancias fueron declaradas antes de medir?
¿Están las condiciones: tensión medida, canales, sustrato por zona y profundidad de inserción?
Dos pares idénticos hasta el último decimal no son un buen resultado: son la
señal de que se calibró una zona y se copió a la otra.
Y un punto de aire en el tope del convertidor invalida la escala completa, por
mucho que los porcentajes resultantes parezcan razonables.
