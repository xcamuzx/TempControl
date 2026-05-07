#!/usr/bin/env python3
"""One-shot SHT3x read on Pi 5 I2C bus 1, address 0x44."""

import time
from smbus2 import SMBus, i2c_msg

I2C_BUS = 1
SENSOR_ADDR = 0x44
CMD_SINGLE_HIGHREP = [0x2C, 0x06]
MEASUREMENT_DELAY_S = 0.020


def crc8(data: bytes) -> int:
    crc = 0xFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x31) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def read_sht3x() -> tuple[float, float]:
    with SMBus(I2C_BUS) as bus:
        bus.write_i2c_block_data(SENSOR_ADDR, CMD_SINGLE_HIGHREP[0], CMD_SINGLE_HIGHREP[1:])
        time.sleep(MEASUREMENT_DELAY_S)
        read = i2c_msg.read(SENSOR_ADDR, 6)
        bus.i2c_rdwr(read)
        data = bytes(read)

    t_raw, t_crc = (data[0] << 8) | data[1], data[2]
    h_raw, h_crc = (data[3] << 8) | data[4], data[5]
    if crc8(data[0:2]) != t_crc or crc8(data[3:5]) != h_crc:
        raise IOError("SHT3x CRC mismatch")

    temp_c = -45.0 + 175.0 * t_raw / 65535.0
    humidity = 100.0 * h_raw / 65535.0
    return temp_c, humidity


if __name__ == "__main__":
    t, h = read_sht3x()
    print(f"Temperature: {t:.2f} °C")
    print(f"Humidity:    {h:.2f} %RH")
