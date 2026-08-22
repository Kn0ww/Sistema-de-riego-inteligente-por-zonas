// ============================================================
// Fundamentos de IoT 2026-2 · Semana 4 (17 al 21 de agosto de 2026)
// P8 · Sistema de riego inteligente por zonas · E03 · LAB 1
// Sensor: humedad de suelo capacitivo (x2) · Familia A      v1.1
// GT2 item 1 — Verificacion fisica del sensor
//
// PASO 3 — Conversion con el par de cada zona y filtrado
// Objetivo unico: transformar milivolts en porcentaje de humedad usando
// el par (m, b) propio de cada zona, y aplicar la media movil de la
// Semana 3. Las dos zonas se leen en el mismo lazo, sin bloquearse.
// ============================================================

// ---------- 1. Configuracion ----------
const int N_ZONAS = 2;                    // zonas montadas
const int PIN_ZONA[N_ZONAS] = {32, 33};
const unsigned long PERIODO_MS = 1000;
const int N_FILTRO = 5;                   // media movil, N impar

// --- Pares (m, b) obtenidos en el paso 2, en % por milivolt. ---
const float M_ZONA[N_ZONAS] = {-0.0454236, -0.0380531};   // <-- REEMPLAZAR
const float B_ZONA[N_ZONAS] = {141.9032, 118.9187};      // <-- REEMPLAZAR

const int MV_MAXIMO_UTIL = 2450;
const int MV_MINIMO_UTIL = 150;

// ---------- 2. Estado interno ----------
unsigned long t_previo = 0;
int  ventana[N_ZONAS][N_FILTRO];
int  indice[N_ZONAS] = {0, 0};
bool llena[N_ZONAS]  = {false, false};

// ---------- 3. Funciones auxiliares ----------
int leer_mv(int pin) {
  long suma = 0;
  for (int i = 0; i < 8; i++) suma += analogReadMilliVolts(pin);
  return (int)(suma / 8);
}

int filtrar(int z, int valor) {           // media movil por zona
  ventana[z][indice[z]] = valor;
  indice[z] = (indice[z] + 1) % N_FILTRO;
  if (indice[z] == 0) llena[z] = true;

  int tope = llena[z] ? N_FILTRO : indice[z];
  long suma = 0;
  for (int i = 0; i < tope; i++) suma += ventana[z][i];
  return (int)(suma / tope);
}

float a_porcentaje(int z, int mv) {
  float h = M_ZONA[z] * mv + B_ZONA[z];
  if (h < 0)   h = 0;                     // el modelo no vale fuera de
  if (h > 100) h = 100;                   // los dos puntos que lo definen
  return h;
}

// El valor SIN recortar sirve para diagnosticar: si la lectura se pasa
// de 100 o baja de 0, la referencia de calibracion ya no reproduce.
float sin_recortar(int z, int mv) {
  return M_ZONA[z] * mv + B_ZONA[z];
}

// ---------- 4. Programa ----------
void setup() {
  Serial.begin(115200);
  for (int z = 0; z < N_ZONAS; z++) analogSetPinAttenuation(PIN_ZONA[z], ADC_11db);
  Serial.println("P8 paso 3 - conversion y filtrado por zona (v1.1)");
  Serial.println("Verificar con un TERCER punto: suelo humedo intermedio.");
}

void loop() {
  unsigned long ahora = millis();

  if (ahora - t_previo >= PERIODO_MS) {
    t_previo = ahora;

    for (int z = 0; z < N_ZONAS; z++) {
      int crudo = leer_mv(PIN_ZONA[z]);
      int filtrado = filtrar(z, crudo);
      float real = sin_recortar(z, filtrado);

      Serial.printf("Z%d: %4d -> %4d mV = %5.1f %%", z + 1, crudo, filtrado,
                    a_porcentaje(z, filtrado));
      if (real > 100.5 || real < -0.5) {
        Serial.printf("  (sin recortar: %.1f %%)", real);
      }
      if (filtrado > MV_MAXIMO_UTIL || filtrado < MV_MINIMO_UTIL) {
        Serial.print("  FUERA DE LA ZONA UTIL");
      }
      Serial.print(" | ");
    }
    Serial.println();
  }
}

// ------------------------------------------------------------
// El tercer punto de verificacion:
//   Con el sensor en tierra humeda, el porcentaje debe caer en un valor
//   intermedio coherente con lo que se observa. No hay patron de
//   humedad en el laboratorio, de modo que la verificacion es de
//   COHERENCIA: cerca de 0 % al aire, cerca de 100 % en agua, y un
//   valor intermedio estable y repetible en tierra humeda.
//
// Por que se muestra el valor SIN recortar cuando se sale de rango:
//   El recorte a 0 y 100 protege a la maquina de estados de valores sin
//   sentido, pero tambien ESCONDE informacion. Si al sumergir el sensor
//   el valor real es 106 %, eso significa que la lectura de hoy es mas
//   baja que la del dia de la calibracion: la referencia no reproduce.
//   Un 100 % en pantalla no distingue entre "llego justo" y "se paso":
//   por eso el programa lo dice cuando ocurre.
//
// Si el valor real se sale de rango de forma sistematica, no se corrige
// el recorte: se repite la calibracion controlando la profundidad de
// insercion y el tiempo de estabilizacion.
// ------------------------------------------------------------
