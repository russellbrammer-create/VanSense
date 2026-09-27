#pragma once
#include <stdint.h>

#define SENSE_MAGIC 0xA5
#define SENSE_VER   3

#define SENSE_F_FROST   0x01
#define SENSE_F_OVERREV 0x02
#define SENSE_F_DUMMY   0x80

#ifndef SENSE_OVERREV_RPM
#define SENSE_OVERREV_RPM 3500
#endif
#ifndef SENSE_FROST_CX10
#define SENSE_FROST_CX10 20
#endif

#ifndef PIN_DS18_OUT
#define PIN_DS18_OUT 4
#endif
#ifndef PIN_DS18_COOL
#define PIN_DS18_COOL 5
#endif
#ifndef PIN_RPM
#define PIN_RPM 6
#endif
#ifndef PIN_TC_BLOCK
#define PIN_TC_BLOCK 7
#endif
#ifndef PIN_BMP_SDA
#define PIN_BMP_SDA 8
#endif
#ifndef PIN_BMP_SCL
#define PIN_BMP_SCL 9
#endif
#ifndef SENSE_RPM_PPR
#define SENSE_RPM_PPR 4
#endif
#ifndef PIN_RPM_SIM
#define PIN_RPM_SIM 10
#endif

struct __attribute__((packed)) SensePkt {
  uint8_t  magic;
  uint8_t  ver;
  int16_t  t_block_x10;
  int16_t  t_cool_x10;
  int16_t  t_out_x10;
  uint16_t rpm;
  uint16_t kpa_x10;
  uint16_t vbat_x100;
  uint8_t  flags;
  int16_t  alt_m;
  uint16_t hpa_x10;
};
