#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr int kI2cSdaPin = 32;  // M5Stack Core2 Port A yellow wire.
constexpr int kI2cSclPin = 33;  // M5Stack Core2 Port A white wire.
constexpr uint32_t kI2cFrequencyHz = 100000;
constexpr uint8_t kPrimarySensorAddress = 0x44;
constexpr uint8_t kAlternateSensorAddress = 0x45;
constexpr uint8_t kSingleShotHighRepeatability[] = {0x2C, 0x06};

uint8_t activeSensorAddress = 0;

uint8_t crc8(const uint8_t *data, size_t length) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                         : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

bool probeAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

uint8_t detectSensorAddress() {
  if (probeAddress(kPrimarySensorAddress)) {
    return kPrimarySensorAddress;
  }
  if (probeAddress(kAlternateSensorAddress)) {
    return kAlternateSensorAddress;
  }
  return 0;
}

bool readSht31(float &temperatureC, float &humidityRh) {
  if (activeSensorAddress == 0) {
    return false;
  }

  Wire.beginTransmission(activeSensorAddress);
  Wire.write(kSingleShotHighRepeatability[0]);
  Wire.write(kSingleShotHighRepeatability[1]);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(20);

  uint8_t data[6] = {};
  const size_t received = Wire.requestFrom(static_cast<int>(activeSensorAddress), 6);
  if (received != sizeof(data)) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  for (uint8_t &byte : data) {
    byte = static_cast<uint8_t>(Wire.read());
  }

  if (crc8(&data[0], 2) != data[2] || crc8(&data[3], 2) != data[5]) {
    return false;
  }

  const uint16_t temperatureRaw = (static_cast<uint16_t>(data[0]) << 8) | data[1];
  const uint16_t humidityRaw = (static_cast<uint16_t>(data[3]) << 8) | data[4];
  temperatureC = -45.0f + (175.0f * static_cast<float>(temperatureRaw) / 65535.0f);
  humidityRh = 100.0f * static_cast<float>(humidityRaw) / 65535.0f;
  return true;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(kI2cSdaPin, kI2cSclPin, kI2cFrequencyHz);
  Serial.println();
  Serial.println("M5Stack Core2 SEN0385 / SHT31 I2C probe");
  Serial.printf("I2C: SDA GPIO%d, SCL GPIO%d, %lu Hz\n", kI2cSdaPin, kI2cSclPin,
                static_cast<unsigned long>(kI2cFrequencyHz));

  activeSensorAddress = detectSensorAddress();
  if (activeSensorAddress == 0) {
    Serial.println("No SEN0385/SHT31 found at 0x44 or 0x45. Check wiring and power.");
    return;
  }

  Serial.printf("Detected SEN0385/SHT31 at I2C address 0x%02X\n", activeSensorAddress);
}

void loop() {
  float temperatureC = 0.0f;
  float humidityRh = 0.0f;

  if (readSht31(temperatureC, humidityRh)) {
    Serial.printf("Temperature: %.2f C | Humidity: %.2f %%RH\n", temperatureC, humidityRh);
  } else {
    Serial.println("Sensor read failed");
  }

  delay(2000);
}
