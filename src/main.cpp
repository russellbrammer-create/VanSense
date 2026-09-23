#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include "sense_pkt.h"

static SensePkt pkt;
static BLEAdvertising *adv = nullptr;
static uint32_t tDummy = 0;
static uint32_t tAdvert = 0;
static uint32_t tDsReq = 0;
static float dummyRpm = 800;
static bool outLive = false;

static OneWire oneWire(PIN_DS18_OUT);
static DallasTemperature ds18(&oneWire);

static String make_mfg() {
  const uint8_t cid[2] = { 0xFF, 0xFF };
  String md;
  md.reserve(2 + sizeof(pkt));
  md.concat((const char *)cid, 2);
  md.concat((const char *)&pkt, (unsigned int)sizeof(pkt));
  return md;
}

static void apply_advert() {
  BLEAdvertisementData data;
  data.setName("VanSense");
  data.setManufacturerData(make_mfg());
  BLEAdvertisementData scan;
  scan.setName("VanSense");
  adv->setAdvertisementData(data);
  adv->setScanResponseData(scan);
}

static void dummy_tick() {
  uint32_t now = millis();
  float t = now / 1000.0f;
  pkt.magic = SENSE_MAGIC;
  pkt.ver = SENSE_VER;
  pkt.t_block_x10 = (int16_t)lroundf((82.0f + 6.0f * sinf(t / 18.0f)) * 10.0f);
  pkt.t_cool_x10  = (int16_t)lroundf((74.0f + 4.0f * sinf(t / 22.0f + 1.0f)) * 10.0f);
  if (!outLive) {
    pkt.t_out_x10 = (int16_t)lroundf((8.0f + 7.0f * sinf(t / 40.0f)) * 10.0f);
  }
  pkt.vbat_x100 = (uint16_t)lroundf((12.55f + 0.12f * sinf(t / 9.0f)) * 100.0f);
  pkt.kpa_x10 = 0;
  if (now - tDummy >= 80) {
    tDummy = now;
    dummyRpm += (float)random(-40, 45);
    if (dummyRpm < 720) dummyRpm = 720;
    if ((now / 12000) % 5 == 4) {
      if (dummyRpm < 3600) dummyRpm += 80;
    } else if (dummyRpm > 980) dummyRpm -= 25;
    pkt.rpm = (uint16_t)lroundf(dummyRpm);
  }
  pkt.flags = outLive ? 0 : SENSE_F_DUMMY;
  if (outLive) pkt.flags |= SENSE_F_DUMMY;
  if (pkt.t_out_x10 <= SENSE_FROST_CX10) pkt.flags |= SENSE_F_FROST;
  if (pkt.rpm >= SENSE_OVERREV_RPM) pkt.flags |= SENSE_F_OVERREV;
}

static void ds18_tick() {
  uint32_t now = millis();
  if (tDsReq == 0) {
    ds18.requestTemperatures();
    tDsReq = now;
    return;
  }
  if (now - tDsReq < 800) return;
  float c = ds18.getTempCByIndex(0);
  tDsReq = 0;
  if (c > -50.0f && c < 85.0f) {
    outLive = true;
    pkt.t_out_x10 = (int16_t)lroundf(c * 10.0f);
  } else {
    outLive = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  memset(&pkt, 0, sizeof(pkt));

  ds18.begin();
  ds18.setWaitForConversion(false);
  ds18.setResolution(12);
  Serial.printf("DS18 devices: %d on GPIO %d\n", ds18.getDeviceCount(), PIN_DS18_OUT);

  dummy_tick();

  BLEDevice::init("VanSense");
  BLEDevice::setPower(ESP_PWR_LVL_P9);
  BLEDevice::createServer();

  adv = BLEDevice::getAdvertising();
  adv->setScanResponse(true);
  adv->setMinInterval(160);
  adv->setMaxInterval(320);
  apply_advert();
  adv->start();

  Serial.println("VanSense advertising v2 + DS18 out");
}

void loop() {
  dummy_tick();
  ds18_tick();
  if (millis() - tAdvert >= 1000) {
    tAdvert = millis();
    apply_advert();
    Serial.printf("out %s %.1fC  blk=%.1f rpm=%u\n",
                  outLive ? "LIVE" : "dummy",
                  pkt.t_out_x10 / 10.0f,
                  pkt.t_block_x10 / 10.0f,
                  pkt.rpm);
  }
  delay(20);
}
