"""SHT3x I2C sensor driver. Pi 5 GPIO header bus = /dev/i2c-1, addr 0x44."""

from __future__ import annotations

import time
from dataclasses import dataclass

from smbus2 import SMBus, i2c_msg

I2C_BUS = 1
SENSOR_ADDR = 0x44
CMD_SINGLE_HIGHREP = (0x2C, 0x06)
MEASUREMENT_DELAY_S = 0.020


class SensorError(Exception):
    pass


@dataclass(frozen=True)
class Reading:
    temp_c: float
    humidity: float
    ts: float


def _crc8(data: bytes) -> int:
    crc = 0xFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x31) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


class Sensor:
    def __init__(
        self,
        bus_num: int = I2C_BUS,
        address: int = SENSOR_ADDR,
        retries: int = 3,
    ) -> None:
        self._bus_num = bus_num
        self._addr = address
        self._retries = retries

    def read(self) -> Reading:
        last_err: Exception | None = None
        for attempt in range(self._retries):
            try:
                return self._read_once()
            except (OSError, SensorError) as e:
                last_err = e
                time.sleep(0.05 * (attempt + 1))
        raise SensorError(f"sensor read failed after {self._retries} attempts: {last_err}")

    def _read_once(self) -> Reading:
        with SMBus(self._bus_num) as bus:
            bus.write_i2c_block_data(self._addr, CMD_SINGLE_HIGHREP[0], [CMD_SINGLE_HIGHREP[1]])
            time.sleep(MEASUREMENT_DELAY_S)
            msg = i2c_msg.read(self._addr, 6)
            bus.i2c_rdwr(msg)
            data = bytes(msg)

        if _crc8(data[0:2]) != data[2] or _crc8(data[3:5]) != data[5]:
            raise SensorError("CRC mismatch")

        t_raw = (data[0] << 8) | data[1]
        h_raw = (data[3] << 8) | data[4]
        return Reading(
            temp_c=-45.0 + 175.0 * t_raw / 65535.0,
            humidity=100.0 * h_raw / 65535.0,
            ts=time.time(),
        )
