// ============================================================
// Fundamentos de IoT 2026 · Semana 4 
// P8 · Sistema de riego inteligente por zonas · E03 
// Sensor: humedad de suelo capacitivo · Familia A 
//============================================================
// ---------- 1. Configuracion ----------

const int ZONA = 1;                       // <-- 1 o 2
const int N_ZONAS = 2;
const int PIN_ZONA[N_ZONAS] = {32, 33};
const int N = 100;                        // muestras por condicion

const int MV_MAXIMO_UTIL   = 2450;        // zona util del convertidor
const int MV_MINIMO_UTIL   = 150;
const int SEPARACION_MINIMA = 250;        // mV entre aire y agua

// ---------- 2. Estado interno ----------
float mv_aire = -1;
float mv_agua = -1;
bool  tope_aire = false;
bool  tope_agua = false;

// ---------- 3. Funciones auxiliares ----------
float promediar(int pin, int n, bool *tope) {
  long suma = 0;
  *tope = false;
  for (int i = 0; i < n; i++) {
    if (analogRead(pin) >= 4090) *tope = true;
    suma += analogReadMilliVolts(pin);
    delay(5);                             // programa de calibracion, no firmware
  }
  return (float)suma / n;
}

bool punto_valido(float mv, bool tope, const char *nombre) {
  if (tope) {
    Serial.printf("\nRECHAZADO: la lectura en %s toco el TOPE del convertidor.\n",
                  nombre);
    Serial.println("La alimentacion del sensor no es 3,3 V, o el modulo entrega");
    Serial.println("mas de lo previsto. Medir AOUT con multimetro y corregir");
    Serial.println("antes de calibrar: un extremo recortado invalida la escala.");
    return false;
  }
  if (mv > MV_MAXIMO_UTIL) {
    Serial.printf("\nAVISO: %s en %.0f mV, sobre la zona util (%d mV).\n",
                  nombre, mv, MV_MAXIMO_UTIL);
    Serial.println("El convertidor comprime por encima de ese valor. Revisar la");
    Serial.println("alimentacion antes de dar el par por bueno.");
    return false;
  }
  if (mv < MV_MINIMO_UTIL) {
    Serial.printf("\nAVISO: %s en %.0f mV, bajo la zona util (%d mV).\n",
                  nombre, mv, MV_MINIMO_UTIL);
    return false;
  }
  return true;
}

void informar_par() {
  if (mv_aire < 0 || mv_agua < 0) return;

  Serial.println();
  Serial.printf("=== Zona %d ===\n", ZONA);
  Serial.printf("Aire (0 %%)  : %.1f mV\n", mv_aire);
  Serial.printf("Agua (100 %%): %.1f mV\n", mv_agua);
  Serial.printf("Separacion  : %.1f mV\n", mv_aire - mv_agua);

  bool ok_aire = punto_valido(mv_aire, tope_aire, "el aire");
  bool ok_agua = punto_valido(mv_agua, tope_agua, "el agua");

  if (fabs(mv_aire - mv_agua) < SEPARACION_MINIMA) {
    Serial.printf("\nAVISO: los dos puntos estan a menos de %d mV. El sensor casi\n",
                  SEPARACION_MINIMA);
    Serial.println("no discrimina: revisar la profundidad de insercion en agua y");
    Serial.println("la tension de alimentacion.");
    ok_agua = false;
  }

  if (!ok_aire || !ok_agua) {
    Serial.println("\nNo se entrega el par: corregir lo anterior y repetir.");
    return;
  }

  // aire = 0 %, agua = 100 %  ->  humedad = m * mV + b
  float m = 100.0 / (mv_agua - mv_aire);
  float b = -m * mv_aire;

  Serial.println();
  Serial.printf("const float M_ZONA%d = %.6f;\n", ZONA, m);
  Serial.printf("const float B_ZONA%d = %.4f;\n", ZONA, b);
  Serial.println();
  Serial.println("Copiar estas dos lineas al paso 3.");
}
// ---------- 4. Programa ----------
void setup() {
  Serial.begin(115200);
  analogSetPinAttenuation(PIN_ZONA[ZONA - 1], ADC_11db);
  Serial.printf("\nP8 paso 2 - calibracion de dos puntos, zona %d (v1.1)\n", ZONA);
  Serial.println("Escribir a con el sensor al aire, w con el sensor en agua.");
}
void loop() {
  if (!Serial.available()) return;

  char c = Serial.read();
  int pin = PIN_ZONA[ZONA - 1];
  
  if (c == 'a') {
    Serial.println("Midiendo AIRE. No mover el sensor.");
    mv_aire = promediar(pin, N, &tope_aire);
    Serial.printf("Aire: %.1f mV\n", mv_aire);
    informar_par();
  } else if (c == 'w') {
    Serial.println("Midiendo AGUA. Sumergir SOLO hasta la marca de profundidad.");
    mv_agua = promediar(pin, N, &tope_agua);
    Serial.printf("Agua: %.1f mV\n", mv_agua);
    informar_par();
  }
}
// ------------------------------------------------------------
// Por que m sale NEGATIVA:
//   En aire los milivolts son altos y en agua bajos, de modo que al
//   aumentar la humedad la lectura disminuye. La pendiente negativa no
//   es un error de signo: describe el sentido real del sensor.
//
// Por que el programa puede NEGARSE a entregar el par:
//   Si un extremo toca el tope del convertidor, la escala se
//   construiria sobre un valor que el sensor nunca entrego. Es
//   preferible detener la calibracion ahi que producir un par que
//   parece razonable y no lo es.
//
// La profundidad de insercion es una CONDICION, no un detalle:
//   El sensor mide a lo largo de toda su zona activa. Sumergir tres
//   centimetros o cinco da lecturas muy distintas.
// ------------------------------------------------------------
