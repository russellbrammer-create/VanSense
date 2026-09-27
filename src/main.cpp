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
static uint32_t tDsCool = 0;
static uint32_t tRpm = 0;
static float dummyRpm = 800;
static bool outLive = false;
static bool coolLive = false;
static bool rpmLive = false;
static volatile uint32_t rpmPulses = 0;
static uint32_t lastSimHz = 0;

static OneWire oneWireOut(PIN_DS18_OUT);
static DallasTemperature dsOut(&oneWireOut);
static OneWire oneWireCool(PIN_DS18_COOL);
static DallasTemperature dsCool(&oneWireCool);

static void IRAM_ATTR on_rpm_pulse() {
  uint32_t n = rpmPulses;
  rpmPulses = n + 1;
}

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
  data.setManufacturerData(make_mfg());
  BLEAdvertisementData scan;
  scan.setName("VanSense");
  adv->setAdvertisementData(data);
  adv->setScanResponseData(scan);
}

static void rpm_sim_set(uint16_t rpm) {
  uint32_t hz = ((uint32_t)rpm * (uint32_t)SENSE_RPM_PPR) / 60u;
  if (hz < 8) hz = 8;
  if (hz == lastSimHz) return;
  lastSimHz = hz;
  ledcChangeFrequency(PIN_RPM_SIM, hz, 8);
}

static void dummy_tick() {
  uint32_t now = millis();
  float t = now / 1000.0f;
  pkt.magic = SENSE_MAGIC;
  pkt.ver = SENSE_VER;
  pkt.t_block_x10 = (int16_t)lroundf((82.0f + 6.0f * sinf(t / 18.0f)) * 10.0f);
  if (!coolLive) {
    pkt.t_cool_x10 = (int16_t)lroundf((74.0f + 4.0f * sinf(t / 22.0f + 1.0f)) * 10.0f);
  }
  if (!outLive) {
    pkt.t_out_x10 = (int16_t)lroundf((8.0f + 7.0f * sinf(t / 40.0f)) * 10.0f);
  }
  pkt.vbat_x100 = (uint16_t)lroundf((12.55f + 0.12f * sinf(t / 9.0f)) * 100.0f);
  pkt.kpa_x10 = 0;
  pkt.alt_m = 0;
  pkt.hpa_x10 = 0;
  if (now - tDummy >= 80) {
    tDummy = now;
    dummyRpm += (float)random(-40, 45);
    if (dummyRpm < 720) dummyRpm = 720;
    if ((now / 12000) % 5 == 4) {
      if (dummyRpm < 3600) dummyRpm += 80;
    } else if (dummyRpm > 980) dummyRpm -= 25;
    if (!rpmLive) pkt.rpm = (uint16_t)lroundf(dummyRpm);
    rpm_sim_set((uint16_t)lroundf(dummyRpm));
  }
  pkt.flags = SENSE_F_DUMMY;
  if (pkt.t_out_x10 <= SENSE_FROST_CX10) pkt.flags |= SENSE_F_FROST;
  if (pkt.rpm >= SENSE_OVERREV_RPM) pkt.flags |= SENSE_F_OVERREV;
}

static void rpm_tick() {
  uint32_t now = millis();
  if (now - tRpm < 250) return;
  uint32_t dt = now - tRpm;
  tRpm = now;
  noInterrupts();
  uint32_t n = rpmPulses;
  rpmPulses = 0;
  interrupts();
  if (n >= 4) {
    uint32_t rpm = (n * 60000ul) / (dt * (uint32_t)SENSE_RPM_PPR);
    if (rpm > 8000) rpm = 8000;
    pkt.rpm = (uint16_t)rpm;
    rpmLive = true;
  } else {
    rpmLive = false;
  }
}

static bool ds_sample(DallasTemperature &bus, int16_t *outx10, uint32_t *stamp) {
  uint32_t now = millis();
  if (*stamp == 0) {
    bus.requestTemperatures();
    *stamp = now;
    return false;
  }
  if (now - *stamp < 800) return false;
  float c = bus.getTempCByIndex(0);
  *stamp = 0;
  if (c > -50.0f && c < 85.0f) {
    *outx10 = (int16_t)lroundf(c * 10.0f);
    return true;
  }
  return false;
}

static void ds18_tick() {
  int16_t v = 0;
  if (ds_sample(dsOut, &v, &tDsReq)) {
    outLive = true;
    pkt.t_out_x10 = v;
  } else if (tDsReq == 0 && !outLive) {
    /* keep dummy until a good sample */
  }
  v = 0;
  if (ds_sample(dsCool, &v, &tDsCool)) {
    coolLive = true;
    pkt.t_cool_x10 = v;
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  memset(&pkt, 0, sizeof(pkt));

  dsOut.begin();
  dsOut.setWaitForConversion(false);
  dsOut.setResolution(12);
  dsCool.begin();
  dsCool.setWaitForConversion(false);
  dsCool.setResolution(12);

  pinMode(PIN_RPM, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_RPM), on_rpm_pulse, FALLING);
  ledcAttach(PIN_RPM_SIM, 53, 8);
  ledcWrite(PIN_RPM_SIM, 128);

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

  Serial.println("VanSense v3 + DS18 out/cool + RPM sim GPIO10");
}

void loop() {
  dummy_tick();
  ds18_tick();
  rpm_tick();
  if (millis() - tAdvert >= 1000) {
    tAdvert = millis();
    apply_advert();
    Serial.printf("out %s %.1f  cool %s %.1f  rpm %s %u\n",
                  outLive ? "LIVE" : "dummy", pkt.t_out_x10 / 10.0f,
                  coolLive ? "LIVE" : "dummy", pkt.t_cool_x10 / 10.0f,
                  rpmLive ? "OPTO" : "dummy", pkt.rpm);
  }
  delay(20);
}
