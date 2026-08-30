/* =========================================================================
   PROYECTO: Sistema de Riego Automatizado con ESP32
   FSM + conversión a % + filtro + LCD 16x2 + relé + LED + buzzer + botón
   SENSOR:
   - 1 sensor de humedad en GPIO 34 (ADC1)
   FSM:
   VIGILANDO
      P < 30%  -> REGANDO
   REGANDO
      P > 40%  -> VIGILANDO
      P < 20%  -> ALERTA
   ALERTA
      P > 25%  -> REGANDO
   ERROR_SEGURO
      2 lecturas seguidas al 100% o botón de paro
      botón de rearme -> VIGILANDO
   No se utiliza delay().
   ========================================================================= */
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// ==========================================================================
// 1. PINES
// ==========================================================================
const uint8_t PIN_SENSOR_HUMEDAD = 34;  // ADC1
const uint8_t PIN_ENERGIA_SENSOR = 32;  
const uint8_t PIN_RELE           = 26;
const uint8_t PIN_LED_VERDE      = 14;
const uint8_t PIN_LED_ROJO       = 27;
const uint8_t PIN_BUZZER         = 25;
const uint8_t PIN_BOTON          = 33;
// ==========================================================================
// 2. RELÉ
// ==========================================================================
// LOW  = relé activado
// HIGH = relé desactivado
const uint8_t RELE_ON  = LOW;
const uint8_t RELE_OFF = HIGH;
// ==========================================================================
// 3. LCD
// ==========================================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
// ==========================================================================
// 4. ADQUISICIÓN Y FILTRADO
// ==========================================================================
// Periodo de muestreo: 60000 ms = 1 minuto
// const uint32_t PERIODO_SENSOR_MS = 60000;
// Media móvil de 5 muestras
const uint8_t N_FILTRO = 5;
// Calibración de tu sensor de la GT1.
// % = M * mV + B
const float M = -0.0454236f;
const float B = 141.9032f;
// Rango útil observado en la calibración
const int MV_MAXIMO_UTIL = 2450;
const int MV_MINIMO_UTIL = 150;
// ==========================================================================
// 5. UMBRALES DE LA FSM
// ==========================================================================
const float P_VIGILANDO_REGANDO = 30.0f;
const float P_REGANDO_VIGILANDO = 40.0f;

const float P_REGANDO_ALERTA    = 20.0f;
const float P_ALERTA_REGANDO    = 25.0f;
// ========================================================================
// 6. MÁQUINA DE ESTADOS
// ==========================================================================
enum Estado : uint8_t {
  VIGILANDO,
  REGANDO,
  ALERTA,
  ERROR_SEGURO
};
Estado estado = VIGILANDO;
// ==========================================================================
// 7. TEMPORIZACIÓN
// ==========================================================================
// Mrca guardada al entrar al estado.
// La clase usa exactamente este patrón.
uint32_t t_entrada = 0;
// Temporización del sensor
uint32_t t_sensor = 0;
// Temporización del botón para antirrebote
uint32_t t_boton = 0;
const uint32_t DEBOUNCE_MS = 50;
// Temporización del buzzer
uint32_t t_buzzer = 0;
const uint32_t PERIODO_BUZZER_MS = 300;

bool buzzerEstado = false;

uint32_t t_lcd = 0;
const uint32_t PERIODO_LCD_MS = 250;
// ==========================================================================
// 8. FILTRO
// ==========================================================================
int ventana[N_FILTRO] = {0};

uint8_t indiceFiltro = 0;
bool filtroLleno = false;
// ==========================================================================
// 9. VARIABLES DEL SENSOR Y SUB-MÁQUINA
// ==========================================================================
enum EstadoSensor : uint8_t {
  SENSOR_REPOSO,
  SENSOR_ESPERANDO_300MS
};

EstadoSensor estadoSensor = SENSOR_REPOSO;
uint32_t t_encendido_sensor = 0;

int lecturaCruda_mV = 0;
int lecturaFiltrada_mV = 0;

float humedadPorcentaje = 0.0f;
float humedadSinRecortar = 0.0f;
// ==========================================================================
// 10. ERROR POR PERSISTENCIA
// ==========================================================================
// Se necesitan 2 lecturas consecutivas al 100%.
uint8_t lecturas100 = 0;
// ==========================================================================
// 11. ESTADO DEL BOTÓN
// ==========================================================================
bool botonEstable = HIGH;
bool botonAnterior = HIGH;
// ==========================================================================
// 12. LEER SENSOR EN mV
// ==========================================================================
int leer_mV() {
  long suma = 0;
  // Promedio de 8 lecturas para reducir variación instantánea
  for (uint8_t i = 0; i < 8; i++) {
    suma += analogReadMilliVolts(PIN_SENSOR_HUMEDAD);
  }
  return (int)(suma / 8);
}
// ==========================================================================
// 13. FILTRO DE MEDIA MÓVIL
// ==========================================================================
int filtrar(int valor) {
  ventana[indiceFiltro] = valor;

  indiceFiltro = (indiceFiltro + 1) % N_FILTRO;

  if (indiceFiltro == 0) {
    filtroLleno = true;
  }
  uint8_t cantidad;

  if (filtroLleno) {
    cantidad = N_FILTRO;
  } else {
    cantidad = indiceFiltro;
  }
  if (cantidad == 0) {
    cantidad = 1;
  }
  long suma = 0;
  for (uint8_t i = 0; i < cantidad; i++) {
    suma += ventana[i];
  }
  return (int)(suma / cantidad);
}
// ==========================================================================
// 14. CONVERSIÓN mV -> PORCENTAJE
// ==========================================================================
float convertirPorcentaje(int mv) {
  float porcentaje = M * mv + B;
  // Guardamos el valor sin limitar para diagnóstico
  humedadSinRecortar = porcentaje;
  // Limitar al rango 0 ... 100 %
  if (porcentaje < 0.0f) {
    porcentaje = 0.0f;
  }
  if (porcentaje > 100.0f) {
    porcentaje = 100.0f;
  }
  return porcentaje;
}
// ==========================================================================
// 15. NOMBRE DEL ESTADO
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
      return "ERROR_SEGURO";
  }
  return "DESCONOCIDO";
}
// ==========================================================================
// 16. CAMBIO DE ESTADO
// ==========================================================================
// ÚNICA función que cambia "estado".
// También reinicia el temporizador de entrada.
void cambiar(Estado nuevoEstado) {
  if (estado == nuevoEstado) {
    return;
  }
  estado = nuevoEstado;
  // Marca de entrada al nuevo estado
  t_entrada = millis();
  // Reiniciar temporizador del buzzer
  t_buzzer = millis();
  buzzerEstado = false;

  digitalWrite(PIN_BUZZER, LOW);

  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] -> ");
  Serial.println(nombreEstado(estado));
}
// =========================================================================
// 17. BOTÓN CON ANTIRREBOTE
// ==========================================================================
bool botonPresionado() {
  bool lectura = digitalRead(PIN_BOTON);
  if (lectura != botonAnterior) {
    t_boton = millis();
    botonAnterior = lectura;
  }
  // Esperar solamente mediante millis(), nunca delay()
  if (millis() - t_boton >= DEBOUNCE_MS) {
    if (lectura != botonEstable) {

      botonEstable = lectura;

      // LOW = botón presionado
      if (botonEstable == LOW) {
        return true;
      }
    }
  }
  return false;
}
// ==========================================================================
// 18. BUZZER INTERMITENTE
// ==========================================================================
void buzzerIntermitente() {
  uint32_t ahora = millis();
  // Temporización no bloqueante
  if (ahora - t_buzzer >= PERIODO_BUZZER_MS) {
    t_buzzer = ahora;
    buzzerEstado = !buzzerEstado;
    digitalWrite(PIN_BUZZER, buzzerEstado);
  }
}
// ==========================================================================
// 19. ACTUADORES
// ==========================================================================
void actualizarActuadores() {
  switch (estado) {
    // ------------------------------------------------------
    // VIGILANDO
    // ------------------------------------------------------
    case VIGILANDO:
      digitalWrite(PIN_RELE, RELE_OFF);

      digitalWrite(PIN_LED_VERDE, HIGH);
      digitalWrite(PIN_LED_ROJO, LOW);

      digitalWrite(PIN_BUZZER, LOW);

      break;
    // ------------------------------------------------------
    // REGANDO
    // ------------------------------------------------------
    case REGANDO:
      digitalWrite(PIN_RELE, RELE_ON);

      digitalWrite(PIN_LED_VERDE, HIGH);
      digitalWrite(PIN_LED_ROJO, LOW);

      digitalWrite(PIN_BUZZER, LOW);
      break;
    // ------------------------------------------------------
    // ALERTA
    // ------------------------------------------------------
    case ALERTA:
      digitalWrite(PIN_RELE, RELE_ON);

      digitalWrite(PIN_LED_VERDE, LOW);
      digitalWrite(PIN_LED_ROJO, HIGH);

      buzzerIntermitente();

      break;
    // ------------------------------------------------------
    // ERROR SEGURO
    // ------------------------------------------------------
    case ERROR_SEGURO:
      // Salida segura:
      // relé apagado
      digitalWrite(PIN_RELE, RELE_OFF);

      digitalWrite(PIN_LED_VERDE, LOW);
      digitalWrite(PIN_LED_ROJO, HIGH);

      buzzerIntermitente();

      break;
  }
}
// ==========================================================================
// 20. LCD
// ==========================================================================
void actualizarLCD() {
  uint32_t ahora = millis();
  
  // ---> Temporizador del LCD <---
  if (ahora - t_lcd >= PERIODO_LCD_MS) {
    t_lcd = ahora;

    // --------- Primera fila ----------
    lcd.setCursor(0, 0);
    lcd.print("Humedad: ");
    lcd.print(humedadPorcentaje, 1);
    lcd.print("%   ");
    
    // ---------- Segunda fila ----------
    lcd.setCursor(0, 1);
    switch (estado) {
      case VIGILANDO:
        lcd.print("Vigilando      ");
        break;
      case REGANDO:
        lcd.print("Regando        ");
        break;
      case ALERTA:
        lcd.print("ALERTA         ");
        break;
      case ERROR_SEGURO:
        lcd.print("ERROR SEGURO   ");
        break;
    }
  }
}
// ==========================================================================
// 21. ACTUALIZACIÓN DEL SENSOR (SUB-MÁQUINA)
// ==========================================================================
void actualizarSensor() {
  uint32_t ahora = millis();

  // ---> NUEVA LÓGICA DE TIEMPO DINÁMICO <---
  uint32_t periodoActual;
  if (estado == VIGILANDO) {
    periodoActual = 60000; // 60 segundos (1 minuto) en VIGILANDO
  } else {
    periodoActual = 1000;  // 1 segundo en el resto de estados
  }

  switch (estadoSensor) {
    
    // ------------------------------------------------------
    // CASO 1: El sensor está apagado esperando su turno
    // ------------------------------------------------------
    case SENSOR_REPOSO:
      // Usamos la variable dinámica "periodoActual"
      if (ahora - t_sensor >= periodoActual) {
        t_sensor = ahora; 
        
        // ENCENDER SENSOR
        digitalWrite(PIN_ENERGIA_SENSOR, HIGH);
        t_encendido_sensor = ahora;
        estadoSensor = SENSOR_ESPERANDO_300MS;
      }
      break;

    // ------------------------------------------------------
    // CASO 2: El sensor está encendido, esperando estabilizarse
    // ------------------------------------------------------
    case SENSOR_ESPERANDO_300MS:
      if (ahora - t_encendido_sensor >= 300) {
        
        // MEDIR Y APAGAR SENSOR
        lecturaCruda_mV = leer_mV();
        digitalWrite(PIN_ENERGIA_SENSOR, LOW);
        
        // Filtrado y conversión
        lecturaFiltrada_mV = filtrar(lecturaCruda_mV);
        humedadPorcentaje = convertirPorcentaje(lecturaFiltrada_mV);

        // Diagnóstico de dos lecturas al 100 %
        if (humedadPorcentaje >= 99.9f) {
          lecturas100++;
        } else {
          lecturas100 = 0;
        }

        // Monitor serie
        Serial.print(" mV | Humedad: ");
        Serial.print(humedadPorcentaje, 1);
        Serial.println(" %");

        // Volver al estado de reposo para el siguiente ciclo
        estadoSensor = SENSOR_REPOSO;
      }
      break;
  }
}
// ==========================================================================
// 22. LÓGICA DE LA FSM
// ==========================================================================
void ejecutarFSM() {
  // ========================================================================
  // EVENTOS GLOBALES
  // Aplican desde cualquier estado excepto ERROR_SEGURO
  // ========================================================================
  if (estado != ERROR_SEGURO) {
    // ------------------------------------------------------
    // Botón de paro
    // ------------------------------------------------------
    if (botonPresionado()) {
      cambiar(ERROR_SEGURO);
      return;
    }
    // ------------------------------------------------------
    // Dos lecturas consecutivas al 100 %
    // ------------------------------------------------------
    if (lecturas100 >= 2) {
      lecturas100 = 0;
      cambiar(ERROR_SEGURO);
      return;
    }
  }
  // ========================================================================
  // FSM: UN CASE POR CADA ESTADO
  // ========================================================================
  switch (estado) {
    // ======================================================================
    // VIGILANDO
    // ======================================================================
    case VIGILANDO:
      // P < 30 % -> REGANDO
      if (humedadPorcentaje < P_VIGILANDO_REGANDO) {
        cambiar(REGANDO);
      }
      break;
    // ======================================================================
    // REGANDO
    // ======================================================================
    case REGANDO:
      // P > 40 % -> VIGILANDO
      if (humedadPorcentaje > P_REGANDO_VIGILANDO) {
        cambiar(VIGILANDO);
      }
      // P < 20 % -> ALERTA
      else if (humedadPorcentaje < P_REGANDO_ALERTA) {
        cambiar(ALERTA);
      }
      break;
    // ======================================================================
    // ALERTA
    // ======================================================================
    case ALERTA:
      // P > 25 % -> REGANDO
      if (humedadPorcentaje > P_ALERTA_REGANDO) {
        cambiar(REGANDO);
      }
      break;
    // ======================================================================
    // ERROR SEGURO
    // ======================================================================
    case ERROR_SEGURO:
      // El sistema permanece aquí
      // hasta recibir el botón de rearme.
      if (botonPresionado()) {
        // Reiniciar contador de fallas
        lecturas100 = 0;
        cambiar(VIGILANDO);
      }
      break;
  }
}
// ==========================================================================
// 23. SETUP
// ==========================================================================
void setup() {
  Serial.begin(115200);
  // ------------------------------------------------------------------------
  // Sensor ADC
  // ------------------------------------------------------------------------
  analogSetPinAttenuation(
    PIN_SENSOR_HUMEDAD,
    ADC_11db
  );
  //-------------------------------------------------------------------------
  // Salidas
  // ------------------------------------------------------------------------
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_ENERGIA_SENSOR, OUTPUT);
  digitalWrite(PIN_ENERGIA_SENSOR, LOW); // Inicia apagado
  // ------------------------------------------------------------------------
  // Botón
  // ------------------------------------------------------------------------
  pinMode(PIN_BOTON, INPUT_PULLUP);
  // ------------------------------------------------------------------------
  // Estado seguro inicial
  // ------------------------------------------------------------------------
  digitalWrite(PIN_RELE, RELE_OFF);

  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_ROJO, LOW);

  digitalWrite(PIN_BUZZER, LOW);
  // ------------------------------------------------------------------------
  // LCD
  // ------------------------------------------------------------------------
  lcd.init();
  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Sistema riego");

  lcd.setCursor(0, 1);
  lcd.print("FSM iniciada");
  // ------------------------------------------------------------------------
  // Inicialización del temporizador del estado
  // ------------------------------------------------------------------------
  t_entrada = millis();

  t_sensor = millis();
  t_boton = millis();
  t_buzzer = millis();
  // Estado inicial
  estado = VIGILANDO;
  Serial.println();
  Serial.println("==========================================");
  Serial.println(" SISTEMA DE RIEGO - FSM");
  Serial.println("==========================================");
  Serial.println("Estado inicial: VIGILANDO");
  Serial.println("Sensor: GPIO34");
  Serial.println("Filtro: media movil N=5");
  Serial.println("Muestreo: 60000 ms");
  Serial.println("Sin delay()");
  Serial.println("==========================================");
}
// ==========================================================================
// 24. LOOP PRINCIPAL
// ==========================================================================
void loop() {
  uint32_t ahora = millis();
  // ========================================================================
  // 1. SENSOR
  // ========================================================================
  actualizarSensor();
  // ========================================================================
  // 2. FSM
  // ========================================================================
  ejecutarFSM();
  // ========================================================================
  // 3. ACTUADORES
  // ========================================================================
  actualizarActuadores();
  // ========================================================================
  // 4. LCD
  // ========================================================================
  actualizarLCD();
  // ========================================================================
  // 5. TEMPORIZADOR DEL ESTADO
  // ========================================================================
  // Se consulta en CADA vuelta del loop.
  // No bloquea el programa.
  uint32_t tiempoEnEstado = ahora - t_entrada;
  // Esta variable permite comprobar cuánto tiempo lleva
  // el sistema dentro del estado actual.
  (void)tiempoEnEstado;
}
