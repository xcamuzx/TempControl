"""SHT3x reader for the Milk-V Duo using only the stdlib I2C char device.

The Pi build uses smbus2, which is not on the stock Duo image. Here we talk to
/dev/i2c-N directly with an I2C_SLAVE ioctl plus os.read/os.write -- enough for
the SHT3x single-shot measurement (write command, wait, read 6 bytes). The
CRC-8 verification matches app/sensor.py on the Pi.
"""

from __future__ import annotations

import fcntl
import os
import time
from dataclasses import dataclass

I2C_SLAVE = 0x0703
SENSOR_ADDR = 0x44
CMD_SINGLE_HIGHREP = bytes((0x2C, 0x06))
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
    """Reads an SHT3x on /dev/i2c-{bus_num}. Retries like the Pi driver."""

    def __init__(self, bus_num: int = 0, address: int = SENSOR_ADDR,
                 retries: int = 3) -> None:
        self._dev = f"/dev/i2c-{bus_num}"
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
        fd = os.open(self._dev, os.O_RDWR)
        try:
            fcntl.ioctl(fd, I2C_SLAVE, self._addr)
            os.write(fd, CMD_SINGLE_HIGHREP)
            time.sleep(MEASUREMENT_DELAY_S)
            data = os.read(fd, 6)
        finally:
            os.close(fd)

        if len(data) != 6:
            raise SensorError("short read")
        if _crc8(data[0:2]) != data[2] or _crc8(data[3:5]) != data[5]:
            raise SensorError("CRC mismatch")

        t_raw = (data[0] << 8) | data[1]
        h_raw = (data[3] << 8) | data[4]
        return Reading(
            temp_c=-45.0 + 175.0 * t_raw / 65535.0,
            humidity=100.0 * h_raw / 65535.0,
            ts=time.time(),
        )


class FakeSensor:
    """Deterministic-ish reading for --sim runs with no I2C bus."""

    def __init__(self) -> None:
        self._t0 = time.time()

    def read(self) -> Reading:
        import math

        dt = time.time() - self._t0
        return Reading(
            temp_c=4.0 + 1.5 * math.sin(dt / 6.0),
            humidity=60.0 + 5.0 * math.sin(dt / 11.0),
            ts=time.time(),
        )
