"""FastAPI app for TempControl ice-bath challenge dashboard."""

from __future__ import annotations

import os
import time
from dataclasses import dataclass, field
from typing import Literal

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel, Field, field_validator

from app.sensor import Sensor, SensorError

HERE = os.path.dirname(os.path.abspath(__file__))

MIN_DURATION = 60
MAX_DURATION = 180
MAX_NAMES = 4
MAX_NAME_LEN = 32

app = FastAPI(title="TempControl")
app.mount("/static", StaticFiles(directory=os.path.join(HERE, "static")), name="static")
templates = Jinja2Templates(directory=os.path.join(HERE, "templates"))

sensor = Sensor()


@dataclass
class ChallengeState:
    status: Literal["idle", "running", "finished"] = "idle"
    names: list[str] = field(default_factory=list)
    duration_seconds: int = 0
    started_at: float | None = None
    ends_at: float | None = None

    def remaining_seconds(self) -> float:
        if self.status != "running" or self.ends_at is None:
            return 0.0
        return max(0.0, self.ends_at - time.time())

    def tick(self) -> None:
        if self.status == "running" and self.ends_at is not None and time.time() >= self.ends_at:
            self.status = "finished"

    def reset(self) -> None:
        self.status = "idle"
        self.names = []
        self.duration_seconds = 0
        self.started_at = None
        self.ends_at = None


state = ChallengeState()


class StartRequest(BaseModel):
    names: list[str] = Field(min_length=1, max_length=MAX_NAMES)
    duration_seconds: int = Field(ge=MIN_DURATION, le=MAX_DURATION)

    @field_validator("names")
    @classmethod
    def _validate_names(cls, v: list[str]) -> list[str]:
        cleaned = [n.strip() for n in v]
        if any(not n for n in cleaned):
            raise ValueError("names cannot be empty or whitespace")
        if any(len(n) > MAX_NAME_LEN for n in cleaned):
            raise ValueError(f"each name max {MAX_NAME_LEN} chars")
        return cleaned


def _state_dict() -> dict:
    return {
        "status": state.status,
        "names": state.names,
        "duration_seconds": state.duration_seconds,
        "remaining_seconds": round(state.remaining_seconds(), 1),
        "ends_at": state.ends_at,
    }


@app.get("/", response_class=HTMLResponse)
def index(request: Request):
    return templates.TemplateResponse(request, "index.html")


@app.get("/api/temp")
def api_temp():
    try:
        r = sensor.read()
    except SensorError as e:
        raise HTTPException(status_code=503, detail=str(e))
    return {"temp_c": round(r.temp_c, 2), "humidity": round(r.humidity, 2), "ts": r.ts}


@app.post("/api/challenge/start")
def api_challenge_start(req: StartRequest):
    state.tick()
    if state.status != "idle":
        raise HTTPException(status_code=409, detail=f"cannot start: status is {state.status}")
    now = time.time()
    state.status = "running"
    state.names = req.names
    state.duration_seconds = req.duration_seconds
    state.started_at = now
    state.ends_at = now + req.duration_seconds
    return _state_dict()


@app.get("/api/challenge/state")
def api_challenge_state():
    state.tick()
    return _state_dict()


@app.post("/api/challenge/ack")
def api_challenge_ack():
    state.tick()
    if state.status != "finished":
        raise HTTPException(status_code=409, detail=f"cannot ack: status is {state.status}")
    state.reset()
    return _state_dict()
