#pragma once
#include <stdint.h>

/* Swap the .cpp behind this header to change chips.
 * Today: src/baro_bmp180.cpp  (GY-68, SDA 8 SCL 9)
 * Later: src/baro_bmp280.cpp etc. Same three functions.
 *
 * Engine-bay airflow idea: one baro in the bay. Parked engine-off
 * pressure is the baseline. Running fans / ram air drop (or raise)
 * that number a few tenths of a hPa. baro_delta_hpa() is "now minus
 * rest". Calibrate rest when RPM is 0 for a few seconds.
 */

bool  baro_begin();
bool  baro_tick();          /* true if a fresh sample landed */
bool  baro_ok();
float baro_hpa();
float baro_alt_m();         /* crude, 1013.25 sea-level */
float baro_delta_hpa();     /* now - rest; +ve = higher than parked */
void  baro_set_rest();      /* call when you trust "engine off" */
