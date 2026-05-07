"""MAX17040 fuel gauge driver for Geekworm X1200 UPS. I2C bus 1, addr 0x36."""

from __future__ import annotations

import time
from dataclasses import dataclass

from smbus2 import SMBus

I2C_BUS = 1
GAUGE_ADDR = 0x36
REG_VCELL = 0x02
REG_SOC = 0x04


class UPSError(Exception):
    pass


@dataclass(frozen=True)
class BatteryReading:
    percent: float
    voltage: float
    ts: float


class UPS:
    def __init__(
        self,
        bus_num: int = I2C_BUS,
        address: int = GAUGE_ADDR,
        retries: int = 3,
    ) -> None:
        self._bus_num = bus_num
        self._addr = address
        self._retries = retries

    def read(self) -> BatteryReading:
        last_err: Exception | None = None
        for attempt in range(self._retries):
            try:
                return self._read_once()
            except (OSError, UPSError) as e:
                last_err = e
                time.sleep(0.05 * (attempt + 1))
        raise UPSError(f"UPS read failed after {self._retries} attempts: {last_err}")

    def _read_once(self) -> BatteryReading:
        with SMBus(self._bus_num) as bus:
            raw_soc = bus.read_word_data(self._addr, REG_SOC)
            soc_be = ((raw_soc & 0xFF) << 8) | ((raw_soc >> 8) & 0xFF)
            pct = (soc_be >> 8) + (soc_be & 0xFF) / 256.0

            raw_vcell = bus.read_word_data(self._addr, REG_VCELL)
            vcell_be = ((raw_vcell & 0xFF) << 8) | ((raw_vcell >> 8) & 0xFF)
            voltage = ((vcell_be >> 4) & 0x0FFF) * 0.00125

        return BatteryReading(
            percent=round(min(pct, 100.0), 1),
            voltage=round(voltage, 3),
            ts=time.time(),
        )
