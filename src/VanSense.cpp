#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include "sense_pkt.h"

enum SenseState { ST_BOOT, ST_SAMPLE, ST_ADVERT };

static SenseState st = ST_BOOT;
static SensePkt pkt;
static BLEAdvertising *adv = nullptr;
static uint32_t tSample = 0;
static uint32_t tAdvert = 0;
static uint32_t tDummy = 0;
static float dummyRpm = 800;

static void pack_advert() {
  pkt.magic = SENSE_MAGIC;
  pkt.ver = SENSE_VER;
  pkt.flags &= (uint8_t)~(SENSE_F_FROST | SENSE_F_OVERREV);
  if (pkt.t_out_x10 <= SENSE_FROST_CX10) pkt.flags |= SENSE_F_FROST;
  if (pkt.rpm >= SENSE_OVERREV_RPM) pkt.flags |= SENSE_F_OVERREV;
#if SENSE_DUMMY
  pkt.flags |= SENSE_F_DUMMY;
#endif

  String md;
  md.reserve(sizeof(pkt));
  md.concat((const char *)&pkt, sizeof(pkt));

  BLEAdvertisementData data;
  data.setName("VanSense");
  data.setManufacturerData(md);
  adv->setAdvertisementData(data);
}

static void dummy_tick() {
  uint32_t now = millis();
  float t = now / 1000.0f;
  pkt.t_block_x10 = (int16_t)lroundf((82.0f + 6.0f * sinf(t / 18.0f)) * 10.0f);
  pkt.t_cool_x10  = (int16_t)lroundf((74.0f + 4.0f * sinf(t / 22.0f + 1.0f)) * 10.0f);
  pkt.t_out_x10   = (int16_t)lroundf((8.0f + 7.0f * sinf(t / 40.0f)) * 10.0f);
  pkt.vbat_x100   = (uint16_t)lroundf((12.55f + 0.12f * sinf(t / 9.0f)) * 100.0f);
  pkt.kpa_x10     = 0;

  if (now - tDummy >= 80) {
    tDummy = now;
    dummyRpm += (float)random(-40, 45);
    if (dummyRpm < 720) dummyRpm = 720;
    if ((now / 12000) % 5 == 4) {
      if (dummyRpm < 3600) dummyRpm += 80;
    } else if (dummyRpm > 980) {
      dummyRpm -= 25;
    }
    pkt.rpm = (uint16_t)lroundf(dummyRpm);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  memset(&pkt, 0, sizeof(pkt));
  pkt.magic = SENSE_MAGIC;
  pkt.ver = SENSE_VER;

  BLEDevice::init("VanSense");
  adv = BLEDevice::getAdvertising();
  adv->setMinInterval(160);
  adv->setMaxInterval(160);
  pack_advert();
  adv->start();

  st = ST_SAMPLE;
  Serial.println("VanSense dummy advert v2");
}

void loop() {
  uint32_t now = millis();
  switch (st) {
    case ST_BOOT:
      st = ST_SAMPLE;
      break;
    case ST_SAMPLE:
      if (now - tSample >= 200) {
        tSample = now;
#if SENSE_DUMMY
        dummy_tick();
#endif
        st = ST_ADVERT;
      }
      break;
    case ST_ADVERT:
      if (now - tAdvert >= 200) {
        tAdvert = now;
        pack_advert();
      }
      st = ST_SAMPLE;
      break;
  }
  delay(5);
}
