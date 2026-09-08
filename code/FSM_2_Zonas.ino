/* =========================================================================
   PROYECTO: Sistema de Riego Automatizado con ESP32 - 2 ZONAS
   ARQUITECTURA:
   - FSM independiente para ZONA 1
   - FSM independiente para ZONA 2
   - Una bomba común
   - Un servo distribuidor para seleccionar la zona
   FSM DE CADA ZONA:
   VIGILANDO
      Si humedad < 30% -> REGANDO
   REGANDO
      Si humedad > 40% -> VIGILANDO
      Si humedad < 20% -> ALERTA
   ALERTA
      Si humedad > 25% -> REGANDO
   ERROR_SEGURO
      2 lecturas seguidas al 100% en cualquiera de las zonas
      o botón de paro
   REARME:
      Botón -> ambas zonas vuelven a VIGILANDO
   CONTROL DE BOMBA:
      Se enciende si alguna zona está en REGANDO o ALERTA.
   CONTROL DE SERVO:
      Selecciona la zona que necesita riego.
      Si ambas requieren riego, prioriza:
        1. ALERTA
        2. Menor humedad
   CADA SENSOR:
      SENSOR_REPOSO -> ENCENDER -> ESPERAR 300 ms -> MEDIR -> APAGAR
   OLED I2C:
      SSD1306 128x64, dirección 0x3C
      SDA = GPIO21
      SCL = GPIO22
   ========================================================================= */
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
// ==========================================================================
// 1. PINES // Funcionamiento
// ==========================================================================
const uint8_t PIN_SENSOR_Z1       = 35;  // Entrada analógica del sensor de humedad de la zona 1.
const uint8_t PIN_ENERGIA_Z1      = 32;  // Controla la alimentación del sensor Z1.
const uint8_t PIN_SENSOR_Z2       = 34;  // Entrada analógica del sensor de humedad de la zona 2.
const uint8_t PIN_ENERGIA_Z2      = 33;  // Controla la alimentación del sensor Z2.
const uint8_t PIN_RELE            = 26;  // Controla la bomba mediante el relé.
const uint8_t PIN_SERVO           = 18;  // Señal del servo distribuidor
const uint8_t PIN_LED_VERDE       = 14;  // Indica funcionamiento normal/riego.
const uint8_t PIN_LED_ROJO        = 27;  // Indica una condición de alerta/error.
const uint8_t PIN_BUZZER          = 25;  // Genera la señal sonora.
const uint8_t PIN_BOTON           = 13;  // Botón de control y rearme.
const uint8_t PIN_SDA             = 21; // Comunicación I2C con el OLED.
const uint8_t PIN_SCL             = 22; // Comunicación I2C con el OLED.
// ==========================================================================
// 2. RELÉ
// ==========================================================================
// Aca se define cómo se debe interpretar la señal enviada al módulo relé para encender o apagar la bomba.
const uint8_t RELE_ON  = LOW;    // LOW = relé activado → bomba encendida.
const uint8_t RELE_OFF = HIGH;   // HIGH = relé desactivado → bomba apagada.
// ==========================================================================
// 3. SERVO
// ==========================================================================
// Define las posiciones que utilizará el servo para dirigir el agua hacia cada zona de riego.
const int SERVO_Z1     = 40; // 40° → dirige el agua hacia la zona 1.
const int SERVO_Z2     = 140; // 140° → dirige el agua hacia la zona 2.
const int SERVO_NEUTRO = 90; // 90° → posición neutra, sin seleccionar una zona.
Servo servoDistribucion; // Crea el objeto encargado de controlar el servo.
int ultimaZonaServo = 0; // Variable que guarda cuál fue la última zona seleccionada para evitar mover innecesariamente el servo.
// 0 = neutro, 1 = zona 1, 2 = zona 2
// ==========================================================================
// 4. OLED
// ==========================================================================
#define SCREEN_WIDTH 128 // Aca se define la resolución de la pantalla: 128 × 64 píxeles.
#define SCREEN_HEIGHT 64 // Aca se define la resolución de la pantalla: 128 × 64 píxeles.
#define OLED_RESET -1 // Aca el -1 significa que la pantalla no utiliza un pin de reset independiente.
#define OLED_ADDR 0x3C // 0x3C: dirección I2C del OLED.
// Se crea el objeto que permite controlar la pantalla mediante la biblioteca Adafruit_SSD1306.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); 
// ==========================================================================
// 5. ADQUISICIÓN Y FILTRADO
// ==========================================================================
const uint8_t N_FILTRO = 5; // Se establece una media móvil de 5 mediciones para reducir las variaciones de la lectura.
// Indica que, después de encender la alimentación del sensor, se esperan 300 ms antes de realizar la medición, permitiendo que la señal se estabilice.
const uint32_t TIEMPO_ESTABILIZACION_SENSOR_MS = 300;
// Constantes de calibración:
const float M1 = -0.057379f; // Sensor zona 1: % = M1*mV + B1
const float B_1 = 116.1756f;
const float M2 = -0.067404f; // Sensor zona 2: % = M2*mV + B2
const float B2 = 136.3588f;
// ==========================================================================
// 6. UMBRALES DE CADA FSM
// ==========================================================================
// Aca se define los valores de humedad que determinan cuándo cada zona cambia de estado.
const float P_VIGILANDO_REGANDO = 30.0f; // Si la humedad baja de 30%, la zona pasa de VIGILANDO a REGANDO.
const float P_REGANDO_VIGILANDO = 40.0f; // Si la humedad supera 40%, pasa de REGANDO a VIGILANDO.
const float P_REGANDO_ALERTA    = 20.0f; // Si la humedad baja de 20%, pasa de REGANDO a ALERTA.
const float P_ALERTA_REGANDO    = 25.0f; // Si la humedad supera 25%, pasa de ALERTA a REGANDO.
// ==========================================================================
// 7. MÁQUINA DE ESTADOS
// ==========================================================================
// Aca se define los diferentes estados posibles de cada zona de riego.
// VIGILANDO → la zona está siendo supervisada y no necesita riego.
// REGANDO → la zona necesita agua.
// ALERTA → la humedad es demasiado baja.
// ERROR_SEGURO → el sistema detiene el funcionamiento por una condición de seguridad.
enum Estado : uint8_t {VIGILANDO, REGANDO, ALERTA, ERROR_SEGURO};
Estado estadoZ1 = VIGILANDO; // Aca ambas zonas comienzan en VIGILANDO.
Estado estadoZ2 = VIGILANDO; // Aca ambas zonas comienzan en VIGILANDO.
// ==========================================================================
// 8. TEMPORIZACIÓN FSM
// ==========================================================================
// Guarda el momento en que cada zona entra a un nuevo estado.
uint32_t t_entradaZ1 = 0; // → almacena el tiempo de entrada al estado actual de Z1.
uint32_t t_entradaZ2 = 0; // → almacena el tiempo de entrada al estado actual de Z2.
// ==========================================================================
// 9. TEMPORIZACIÓN BOTÓN
// ==========================================================================
// Controla el botón y evita que una sola pulsación sea detectada varias veces debido al rebote mecánico del botón.
uint32_t t_boton = 0; // Guarda el momento de la última modificación detectada en el botón.
const uint32_t DEBOUNCE_MS = 50; // Establece un tiempo de 50 ms para confirmar que el cambio del botón es real.
bool botonEstable = HIGH; // → estado confirmado del botón.
bool botonAnterior = HIGH; // → estado que tenía anteriormente.
// ==========================================================================
// 10. BUZZER
// ==========================================================================
// Controla el tiempo y el estado del buzzer para poder hacerlo sonar de manera intermitente.
uint32_t t_buzzer = 0; // Guarda el momento de la última modificación del buzzer.
const uint32_t PERIODO_BUZZER_MS = 300; // Define un período de 300 ms para cambiar el estado del buzzer.
bool buzzerEstado = false; // Indica si el buzzer está actualmente en false → apagado o en true → encendido.
// ==========================================================================
// 11. OLED
// ==========================================================================
// Aca se controla cada cuánto se actualiza la información del OLED, evitando actualizarlo continuamente.
uint32_t t_oled = 0; // Guarda el momento de la última actualización del OLED.
const uint32_t PERIODO_OLED_MS = 250; // Se establece que la pantalla se actualice cada 250 ms.
// ==========================================================================
//OBSERVACIÓN: SE DEFINIERON 
//CONST= NO CAMBIAN EN TODO EL CODIGO Y FLOAT O INT EL TIPO DE VALOR
// ==========================================================================
// ==========================================================================
// 12. SUBMÁQUINA DE CADA SENSOR
// ==========================================================================
// Aca se define los estados utilizados para controlar el proceso de medición de cada sensor.
// SENSOR_REPOSO → el sensor está apagado y esperando el momento de realizar una medición.
// SENSOR_ESPERANDO_300MS → el sensor ya fue encendido y está esperando 300 ms antes de medir.
enum EstadoSensor : uint8_t {SENSOR_REPOSO, SENSOR_ESPERANDO_300MS};
EstadoSensor estadoSensorZ1 = SENSOR_REPOSO; // Ambos sensores comienzan en reposo.
EstadoSensor estadoSensorZ2 = SENSOR_REPOSO; // Ambos sensores comienzan en reposo.
uint32_t t_sensorZ1 = 0; // Guardan el tiempo de la última medición de cada sensor.
uint32_t t_sensorZ2 = 0; // Guardan el tiempo de la última medición de cada sensor.
uint32_t t_encendidoZ1 = 0; // Guardan el momento en que se encendió cada sensor para poder comprobar que hayan pasado los 300 ms de estabilización.
uint32_t t_encendidoZ2 = 0; // Guardan el momento en que se encendió cada sensor para poder comprobar que hayan pasado los 300 ms de estabilización.
// ==========================================================================
// 13. FILTROS INDEPENDIENTES
// ==========================================================================
// Se creo un filtro independiente para cada sensor utilizando una media móvil de 5 mediciones.
int ventanaZ1[N_FILTRO] = {0}; // Se crea ventana donde se almacenan las últimas mediciones:
int ventanaZ2[N_FILTRO] = {0}; // Se crea ventana donde se almacenan las últimas mediciones:
// Como N_FILTRO = 5, cada ventana puede almacenar 5 valores.
uint8_t indiceFiltroZ1 = 0; // Indican la posición donde se guardará la siguiente medición.
uint8_t indiceFiltroZ2 = 0; // Indican la posición donde se guardará la siguiente medición.
bool filtroLlenoZ1 = false; // Indican si las 5 posiciones del filtro ya fueron utilizadas.
bool filtroLlenoZ2 = false; // Indican si las 5 posiciones del filtro ya fueron utilizadas.
// ==========================================================================
// 14. VARIABLES DE CADA SENSOR
// ==========================================================================
// Aca se almacena los valores obtenidos y procesados de cada sensor.
int lecturaCrudaZ1_mV = 0; // Guarda la lectura original del sensor en milivoltios (mV).
int lecturaFiltradaZ1_mV = 0; // Guarda la lectura después de aplicar el filtro.
float humedadZ1 = 0.0f; // Guarda el porcentaje de humedad utilizado por la FSM.
float humedadSinRecortarZ1 = 0.0f; // Guarda el porcentaje calculado antes de limitarlo entre 0% y 100%.

int lecturaCrudaZ2_mV = 0;
int lecturaFiltradaZ2_mV = 0;
float humedadZ2 = 0.0f;
float humedadSinRecortarZ2 = 0.0f; 

bool humedadValidaZ1 = false;
bool humedadValidaZ2 = false;
// =========================================================================
// 15. ERROR POR PERSISTENCIA, INDEPENDIENTE POR ZONA
// ==========================================================================
uint8_t lecturas100Z1 = 0;
uint8_t lecturas100Z2 = 0;
// ==========================================================================
// Bueno la clave de entender esto es que es un como un tipo de dato int, la unica diferencia es que 
// 8 bits son datos de 0-255 -> por ende los datos que se le asigne a este valor estara dentro de ese rango
// SE USA PARA REDUCIR USO DE MEMORIA
// uint8_t significa:
// u → unsigned (sin signo, no admite negativos)
// int → entero
// 8 → utiliza 8 bits o 32 → 32 bits
// ==========================================================================
// ==========================================================================
// 16. FUNCIONES DE SENSOR
// ==========================================================================
int leer_mV(uint8_t pinSensor) {
  long suma = 0;
  // Promedio de 8 lecturas para reducir variación instantánea.
  for (uint8_t i = 0; i < 8; i++) {
    suma += analogReadMilliVolts(pinSensor);
  }
  return (int)(suma / 8);
}
int filtrar(int valor, int *ventana, uint8_t &indice, bool &lleno) {
  ventana[indice] = valor;
  indice = (indice + 1) % N_FILTRO;

  if (indice == 0) {
    lleno = true;
  }
  uint8_t cantidad = lleno ? N_FILTRO : indice;
  if (cantidad == 0) {
    cantidad = 1;
  }
  long suma = 0;
  for (uint8_t i = 0; i < cantidad; i++) {
    suma += ventana[i];
  }
  return (int)(suma / cantidad);
}
float convertirPorcentaje(float mv, float M, float B, float &sinRecortar) {
  float porcentaje = M * mv + B;
  sinRecortar = porcentaje;
  if (porcentaje < 0.0f) {
    porcentaje = 0.0f;
  }
  if (porcentaje > 100.0f) {
    porcentaje = 100.0f;
  }
  return porcentaje;
}
// ==========================================================================
// 17. NOMBRE DEL ESTADO
// ==========================================================================
const char* nombreEstado(Estado e) {
  switch (e) {
    case VIGILANDO:
      return "VIGILANDO";
    case REGANDO:
      return "REGANDO";
    case ALERTA:
      return "ALERTA";
    case ERROR_SEGURO:
      return "ERROR SEGURO";
  }
  return "DESCONOCIDO";
}
// ==========================================================================
// 18. CAMBIO DE ESTADO ZONA 1
// ==========================================================================
void cambiarZ1(Estado nuevoEstado) {
  if (estadoZ1 == nuevoEstado) {
    return;
  }
  estadoZ1 = nuevoEstado;
  t_entradaZ1 = millis();
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] Z1 -> ");
  Serial.println(nombreEstado(estadoZ1));
}
// ==========================================================================
// 19. CAMBIO DE ESTADO ZONA 2
// ==========================================================================
void cambiarZ2(Estado nuevoEstado) {
  if (estadoZ2 == nuevoEstado) {
    return;
  }
  estadoZ2 = nuevoEstado;
  t_entradaZ2 = millis();
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] Z2 -> ");
  Serial.println(nombreEstado(estadoZ2));
}
// ==========================================================================
// 20. BOTÓN CON ANTIRREBOTE
// ==========================================================================
bool botonPresionado() {
  bool lectura = digitalRead(PIN_BOTON);
  if (lectura != botonAnterior) {
    t_boton = millis();
    botonAnterior = lectura;
  }
  if (millis() - t_boton >= DEBOUNCE_MS) {
    if (lectura != botonEstable) {
      botonEstable = lectura;
      if (botonEstable == LOW) {
        return true;
      }
    }
  }
  return false;
}
// ==========================================================================
// 21. COMPROBAR ERROR GLOBAL
// ==========================================================================
// Como la bomba y el agua son recursos comunes,
// cualquier error de una zona detiene todo el sistema.
bool errorGlobalActivo() {
  return (
    estadoZ1 == ERROR_SEGURO ||
    estadoZ2 == ERROR_SEGURO
  );
}
// ==========================================================================
// 22. ENTRAR EN ERROR SEGURO
// ==========================================================================
void entrarErrorSeguro() {
  cambiarZ1(ERROR_SEGURO);
  cambiarZ2(ERROR_SEGURO);
  digitalWrite(PIN_RELE, RELE_OFF);
  servoDistribucion.write(SERVO_NEUTRO);
  ultimaZonaServo = 0;
  Serial.println("!!! ERROR SEGURO GLOBAL !!!");
}
// ==========================================================================
// 23. BUZZER INTERMITENTE
// ==========================================================================
void buzzerIntermitente() {
  uint32_t ahora = millis();
  if (ahora - t_buzzer >= PERIODO_BUZZER_MS) { 
    t_buzzer = ahora;
    buzzerEstado = !buzzerEstado;
    digitalWrite(PIN_BUZZER, buzzerEstado);
  }
}
// ==========================================================================
// 24. SELECCIÓN DE ZONA PARA EL SERVO
// ==========================================================================
int seleccionarZonaRiego() {
  bool z1Alerta = (estadoZ1 == ALERTA);
  bool z2Alerta = (estadoZ2 == ALERTA);
  bool z1Regando = (estadoZ1 == REGANDO);
  bool z2Regando = (estadoZ2 == REGANDO);

  if (z1Alerta && z2Alerta) { // Si Z1 y Z2 están en ALERTA
    if (humedadZ1 <= humedadZ2) { // Si Z1 y Z2 están en ALERTA
      return 1;
    } else {
      return 2;
    }
  }
  if (z1Alerta) { // ALERTA tiene prioridad
    return 1;
  }
  if (z2Alerta) { // ALERTA tiene prioridad
    return 2;
  }
  if (z1Regando && z2Regando) { // Si ambas están REGANDO
    if (humedadZ1 <= humedadZ2) { // Si ambas están REGANDO
      return 1;
    } else {
      return 2;
    }
  }
  if (z1Regando) { // Solo una zona está REGANDO
    return 1;
  }
  if (z2Regando) { // Solo una zona está REGANDO
    return 2;
  }
  return 0; // Nadie necesita riego
}
// ==========================================================================
// 25. ACTUALIZACIÓN DEL SERVO
// ==========================================================================
void actualizarServo() {
  int zonaSeleccionada = seleccionarZonaRiego();
  if (zonaSeleccionada == ultimaZonaServo) {
    return;
  }
  switch (zonaSeleccionada) {
    case 0:
      servoDistribucion.write(SERVO_NEUTRO);
      Serial.println("Servo -> NEUTRO");
      break;
    case 1:
      servoDistribucion.write(SERVO_Z1);
      Serial.println("Servo -> ZONA 1");
      break;
    case 2:
      servoDistribucion.write(SERVO_Z2);
      Serial.println("Servo -> ZONA 2");
      break;
  }
  ultimaZonaServo = zonaSeleccionada;
}
// ==========================================================================
// 26. ACTUALIZACIÓN DE ACTUADORES
// ==========================================================================
void actualizarActuadores() {
  if (errorGlobalActivo()) { // ERROR GLOBAL
    digitalWrite(PIN_RELE, RELE_OFF);
    digitalWrite(PIN_LED_VERDE, LOW);
    digitalWrite(PIN_LED_ROJO, HIGH);
    buzzerIntermitente();
    servoDistribucion.write(SERVO_NEUTRO);
    ultimaZonaServo = 0;
    return;
  }

  bool hayAlerta = (estadoZ1 == ALERTA || estadoZ2 == ALERTA); // ALERTA
 
  bool hayRiego = (estadoZ1 == REGANDO || estadoZ2 == REGANDO || estadoZ1 == ALERTA || estadoZ2 == ALERTA); // RIEGO ACTIVO

  if (hayAlerta) { // ALERTA
    digitalWrite(PIN_RELE, RELE_ON);
    digitalWrite(PIN_LED_VERDE, LOW);
    digitalWrite(PIN_LED_ROJO, HIGH);
    buzzerIntermitente();
    actualizarServo();
    return;
  }
  if (hayRiego) { // REGANDO
    digitalWrite(PIN_RELE, RELE_ON);
    digitalWrite(PIN_LED_VERDE, HIGH);
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_BUZZER, LOW);
    buzzerEstado = false;
    actualizarServo();
    return;
  }
  digitalWrite(PIN_RELE, RELE_OFF); // VIGILANDO
  digitalWrite(PIN_LED_VERDE, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  buzzerEstado = false;
  actualizarServo();
}
// ==========================================================================
// 27. OLED
// ==========================================================================
void actualizarOLED() {
  uint32_t ahora = millis();
  if (ahora - t_oled < PERIODO_OLED_MS) {
    return;
  }
  t_oled = ahora;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Z1: ");   // HUMEDAD Z1
  display.print(humedadZ1, 1);
  display.println(" %");
  display.print("Z2: ");   // HUMEDAD Z2
  display.print(humedadZ2, 1);
  display.println(" %");
  display.println();
  display.print("EST Z1: "); // ESTADOS INDEPENDIENTE Z1
  display.println(nombreEstado(estadoZ1));
  display.print("EST Z2: "); // ESTADOS INDEPENDIENTE Z2
  display.println(nombreEstado(estadoZ2));
  display.display();
}
// ==========================================================================
// 28. ACTUALIZACIÓN SENSOR ZONA 1
// ==========================================================================
void actualizarSensorZ1() {
  uint32_t ahora = millis();
  // En error seguro no seguimos energizando el sensor.
  if (estadoZ1 == ERROR_SEGURO) {
    digitalWrite(PIN_ENERGIA_Z1, LOW);
    estadoSensorZ1 = SENSOR_REPOSO;
    return;
  }
  // Cada zona usa el estado de SU propia FSM.
  uint32_t periodoActual =
    (estadoZ1 == VIGILANDO)
    ? 60000UL
    : 1000UL;
  switch (estadoSensorZ1) {
    case SENSOR_REPOSO:
      if (ahora - t_sensorZ1 >= periodoActual) {
        t_sensorZ1 = ahora;
        digitalWrite(PIN_ENERGIA_Z1, HIGH);
        t_encendidoZ1 = ahora;
        estadoSensorZ1 = SENSOR_ESPERANDO_300MS;
      }
      break;
    case SENSOR_ESPERANDO_300MS:
      if (ahora - t_encendidoZ1 >= TIEMPO_ESTABILIZACION_SENSOR_MS) {
        lecturaCrudaZ1_mV = leer_mV(PIN_SENSOR_Z1);
        digitalWrite(PIN_ENERGIA_Z1, LOW);
        lecturaFiltradaZ1_mV = filtrar(lecturaCrudaZ1_mV, ventanaZ1, indiceFiltroZ1, filtroLlenoZ1);
        humedadZ1 = convertirPorcentaje(lecturaFiltradaZ1_mV, M1, B_1, humedadSinRecortarZ1);
        humedadValidaZ1 = true;
        if (humedadZ1 >= 99.9f) { // ERROR: dos lecturas consecutivas al 100%
          if (lecturas100Z1 < 255) {
            lecturas100Z1++;
          }
        } else {
          lecturas100Z1 = 0;
        }
        Serial.print("Z1 | ");
        Serial.print(lecturaCrudaZ1_mV);
        Serial.print(" mV | Filtrado: ");
        Serial.print(lecturaFiltradaZ1_mV);
        Serial.print(" mV | Humedad: ");
        Serial.print(humedadZ1, 1);
        Serial.print(" % | Estado: ");
        Serial.println(nombreEstado(estadoZ1));
        estadoSensorZ1 = SENSOR_REPOSO;
      }
      break;
  }
}
// ==========================================================================
// 29. ACTUALIZACIÓN SENSOR ZONA 2
// ==========================================================================
void actualizarSensorZ2() {
  uint32_t ahora = millis();
  if (estadoZ2 == ERROR_SEGURO) { // En error seguro no seguimos energizando el sensor.
    digitalWrite(PIN_ENERGIA_Z2, LOW);
    estadoSensorZ2 = SENSOR_REPOSO;
    return;
  }
  // Cada zona usa el estado de SU propia FSM.
  uint32_t periodoActual =
    (estadoZ2 == VIGILANDO)
    ? 60000UL
    : 1000UL;
  switch (estadoSensorZ2) {
    case SENSOR_REPOSO:
      if (ahora - t_sensorZ2 >= periodoActual) {
        t_sensorZ2 = ahora;
        digitalWrite(PIN_ENERGIA_Z2, HIGH);
        t_encendidoZ2 = ahora;
        estadoSensorZ2 = SENSOR_ESPERANDO_300MS;
      }
      break;
    case SENSOR_ESPERANDO_300MS:
      if (ahora - t_encendidoZ2 >= TIEMPO_ESTABILIZACION_SENSOR_MS) {
        lecturaCrudaZ2_mV = leer_mV(PIN_SENSOR_Z2);
        digitalWrite(PIN_ENERGIA_Z2, LOW);
        lecturaFiltradaZ2_mV = filtrar(lecturaCrudaZ2_mV, ventanaZ2, indiceFiltroZ2, filtroLlenoZ2);
        humedadZ2 = convertirPorcentaje(lecturaFiltradaZ2_mV, M2, B2, humedadSinRecortarZ2);
        humedadValidaZ2 = true;

        // ERROR: dos lecturas consecutivas al 100%
        if (humedadZ2 >= 99.9f) {
          if (lecturas100Z2 < 255) {
            lecturas100Z2++;
          }
        } else {
          lecturas100Z2 = 0;
        }
        Serial.print("Z2 | ");
        Serial.print(lecturaCrudaZ2_mV);
        Serial.print(" mV | Filtrado: ");
        Serial.print(lecturaFiltradaZ2_mV);
        Serial.print(" mV | Humedad: ");
        Serial.print(humedadZ2, 1);
        Serial.print(" % | Estado: ");
        Serial.println(nombreEstado(estadoZ2));
        estadoSensorZ2 = SENSOR_REPOSO;
      }
      break;
  }
}
// ==========================================================================
// 30. ACTUALIZACIÓN DE AMBOS SENSORES
// ==========================================================================
void actualizarSensores() {
  actualizarSensorZ1();
  actualizarSensorZ2();
}
// ==========================================================================
// 31. REINICIO DE LAS DOS ZONAS
// ==========================================================================
void reiniciarDosZonas() {
  digitalWrite(PIN_ENERGIA_Z1, LOW); // Apagar alimentación de ambos sensores
  digitalWrite(PIN_ENERGIA_Z2, LOW); // Apagar alimentación de ambos sensores
  estadoSensorZ1 = SENSOR_REPOSO; // Reiniciar submáquinas
  estadoSensorZ2 = SENSOR_REPOSO; // Reiniciar submáquinas

  // Forzar nueva medición
  uint32_t ahora = millis();

  t_sensorZ1 = ahora - 60000UL;
  t_sensorZ2 = ahora - 60000UL;

  t_encendidoZ1 = ahora;
  t_encendidoZ2 = ahora;
  // Reiniciar filtros
  for (uint8_t i = 0; i < N_FILTRO; i++) {
    ventanaZ1[i] = 0;
    ventanaZ2[i] = 0;
  }
  indiceFiltroZ1 = 0;
  indiceFiltroZ2 = 0;

  filtroLlenoZ1 = false;
  filtroLlenoZ2 = false;
  // Reiniciar errores
  lecturas100Z1 = 0;
  lecturas100Z2 = 0;

  // Nuevas mediciones necesarias
  humedadValidaZ1 = false;
  humedadValidaZ2 = false;

  humedadZ1 = 0.0f;
  humedadZ2 = 0.0f;
  
  // FSM independientes a VIGILANDO
  estadoZ1 = VIGILANDO;
  estadoZ2 = VIGILANDO;
  t_entradaZ1 = ahora;
  t_entradaZ2 = ahora;

  // Servo a posición neutra
  servoDistribucion.write(SERVO_NEUTRO);
  ultimaZonaServo = 0;
  Serial.println("==========================================");
  Serial.println("REARME DEL SISTEMA");
  Serial.println("Z1 -> VIGILANDO");
  Serial.println("Z2 -> VIGILANDO");
  Serial.println("BOMBA -> APAGADA");
  Serial.println("SERVO -> NEUTRO");
  Serial.println("==========================================");
}
// ==========================================================================
// 32. FSM DE ZONA 1
// ==========================================================================
void ejecutarFSM_Z1() {
  if (estadoZ1 == ERROR_SEGURO) {
    return;
  }
  if (!humedadValidaZ1) {
    return;
  }
  switch (estadoZ1) {
    case VIGILANDO: // VIGILANDO
      if (humedadZ1 < P_VIGILANDO_REGANDO) {
        cambiarZ1(REGANDO);
      }
      break;
    case REGANDO: // REGANDO
      if (humedadZ1 > P_REGANDO_VIGILANDO) {
        cambiarZ1(VIGILANDO);
      }
      else if (humedadZ1 < P_REGANDO_ALERTA) {
        cambiarZ1(ALERTA);
      }
      break;
    case ALERTA: // ALERTA
      if (humedadZ1 > P_ALERTA_REGANDO) {
        cambiarZ1(REGANDO);
      }
      break;
    case ERROR_SEGURO:  // ERROR
      break;
  }
}
// ==========================================================================
// 33. FSM DE ZONA 2
// ==========================================================================
void ejecutarFSM_Z2() {
  if (estadoZ2 == ERROR_SEGURO) {
    return;
  }
  if (!humedadValidaZ2) {
    return;
  }
  switch (estadoZ2) {
    case VIGILANDO: // VIGILANDO
      if (humedadZ2 < P_VIGILANDO_REGANDO) {
        cambiarZ2(REGANDO);
      }
      break;
    case REGANDO: // REGANDO
      if (humedadZ2 > P_REGANDO_VIGILANDO) {
        cambiarZ2(VIGILANDO);
      }
      else if (humedadZ2 < P_REGANDO_ALERTA) {
        cambiarZ2(ALERTA);
      }
      break;
    case ALERTA: // ALERTA
      if (humedadZ2 > P_ALERTA_REGANDO) {
        cambiarZ2(REGANDO);
      }
      break;
    case ERROR_SEGURO: // ERROR
      break;
  }
}
// ==========================================================================
// 34. LÓGICA GENERAL DE LAS DOS FSM
// ==========================================================================
void ejecutarFSM() {
  if (botonPresionado()) { // BOTÓN
    if (errorGlobalActivo()) { // Si ya estamos en error, el botón rearma.
      reiniciarDosZonas();
      return;
    }
    entrarErrorSeguro(); // Si no estamos en error, el botón provoca error seguro.
    return;
  }
  if (!errorGlobalActivo()) { // ERROR POR DOS LECTURAS AL 100%
    if (lecturas100Z1 >= 2 ||
        lecturas100Z2 >= 2) {
      lecturas100Z1 = 0;
      lecturas100Z2 = 0;
      entrarErrorSeguro();
      return;
    }
  }
  if (errorGlobalActivo()) { // Si hay error, no se ejecutan las FSM normales.
    return;
  }
  ejecutarFSM_Z1(); // Cada zona ejecuta SU propia FSM.
  ejecutarFSM_Z2(); // Cada zona ejecuta SU propia FSM.
}
// ==========================================================================
// 35. SETUP
// ==========================================================================
void setup() {
  Serial.begin(115200);

  analogSetPinAttenuation(PIN_SENSOR_Z1,ADC_11db); // ADC
  analogSetPinAttenuation(PIN_SENSOR_Z2,ADC_11db);

  pinMode(PIN_RELE, OUTPUT); // SALIDAS
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_ENERGIA_Z1, OUTPUT);
  pinMode(PIN_ENERGIA_Z2, OUTPUT);
  digitalWrite(PIN_ENERGIA_Z1, LOW);
  digitalWrite(PIN_ENERGIA_Z2, LOW);

  pinMode(PIN_BOTON,INPUT_PULLUP); // BOTÓN

  digitalWrite(PIN_RELE,RELE_OFF); // ESTADO SEGURO INICIAL
  digitalWrite(PIN_LED_VERDE,LOW); // ESTADO SEGURO INICIAL
  digitalWrite(PIN_LED_ROJO,LOW); // ESTADO SEGURO INICIAL
  digitalWrite(PIN_BUZZER,LOW); // ESTADO SEGURO INICIAL

  servoDistribucion.setPeriodHertz(50);  // SERVO
  servoDistribucion.attach(PIN_SERVO,500,2400); // SERVO
  servoDistribucion.write(SERVO_NEUTRO); // SERVO
  ultimaZonaServo = 0;

  Wire.begin(PIN_SDA,PIN_SCL);   // OLED

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,OLED_ADDR
      )) {
    Serial.println("ERROR: no se pudo iniciar el OLED SSD1306.");
  }
  else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Sistema riego");
    display.println("2 zonas");
    display.display();
  }
  // ------------------------------------------------------
  // TEMPORIZADORES
  // ------------------------------------------------------
  uint32_t ahora = millis();

  t_entradaZ1 = ahora;
  t_entradaZ2 = ahora;

  t_sensorZ1 = ahora - 60000UL; // Forzar primera medición de ambas zonas.
  t_sensorZ2 = ahora - 60000UL; // Forzar primera medición de ambas zonas.

  t_boton = ahora;
  t_buzzer = ahora;
  t_oled = ahora;

  estadoZ1 = VIGILANDO; // FSM INICIAL
  estadoZ2 = VIGILANDO; // FSM INICIAL

  Serial.println(); // INFORMACIÓN SERIAL (Es el mensaje que sale en el monitor serial)
  Serial.println(" ========================================== ");
  Serial.println(" SISTEMA DE RIEGO - 2 FSM INDEPENDIENTES ");
  Serial.println(" ========================================== ");
  Serial.println(" Estado Z1: VIGILANDO ");
  Serial.println(" Estado Z2: VIGILANDO ");
  Serial.println(" Z1 sensor: GPIO35 | VCC: GPIO32 ");
  Serial.println(" Z2 sensor: GPIO34 | VCC: GPIO33 ");
  Serial.println(" Bomba/Relay: GPIO26 ");
  Serial.println(" Servo: GPIO18 ");
  Serial.println(" Boton: GPIO13 ");
  Serial.println(" OLED: I2C SDA=21 SCL=22 ");
  Serial.println(" Filtro: media movil N=5 por zona ");
  Serial.println(" Muestreo: 60000 ms en VIGILANDO ");
  Serial.println(" Muestreo: 1000 ms en REGANDO/ALERTA ");
  Serial.println(" Sin delay() en loop ");
  Serial.println(" ========================================== ");
  }
// ==========================================================================
// 36. LOOP PRINCIPAL
// ==========================================================================
void loop() {
  uint32_t ahora = millis();
  actualizarSensores(); // 1. Sensores independientes
  ejecutarFSM(); // 2. FSM Z1 + FSM Z2
  actualizarActuadores();   // 3. Bomba + LEDs + buzzer + servo
  actualizarOLED();  // 4. OLED
  // Supervisión del tiempo en cada FSM.
  uint32_t tiempoZ1 = ahora - t_entradaZ1;
  uint32_t tiempoZ2 = ahora - t_entradaZ2;
  (void)tiempoZ1;
  (void)tiempoZ2;
}
