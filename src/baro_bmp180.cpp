#include <Wire.h>
#include <Adafruit_BMP085.h>
#include "sense_pkt.h"
#include "baro.h"

static Adafruit_BMP085 bmp;
static bool ready = false;
static float hpa = 0;
static float altM = 0;
static float restHpa = 0;
static uint32_t tSample = 0;

bool baro_begin() {
  Wire.begin(PIN_BMP_SDA, PIN_BMP_SCL);
  delay(20);
  ready = bmp.begin();
  if (ready) {
    baro_tick();
    restHpa = hpa;
  }
  return ready;
}

bool baro_tick() {
  if (!ready) return false;
  if (millis() - tSample < 500) return false;
  tSample = millis();
  float p = bmp.readPressure() / 100.0f;
  if (p < 300.0f || p > 1100.0f) return false;
  hpa = p;
  altM = bmp.readAltitude(101325);
  if (restHpa < 300.0f) restHpa = hpa;
  return true;
}

bool  baro_ok()         { return ready; }
float baro_hpa()        { return hpa; }
float baro_alt_m()      { return altM; }
float baro_delta_hpa()  { return ready ? (hpa - restHpa) : 0; }
void  baro_set_rest()   { if (ready) restHpa = hpa; }
