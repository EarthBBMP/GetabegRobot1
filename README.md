# PipeInspect UI

Web control dashboard for a **dual-steer pipe inspection robot**. Drive over Ethernet, steer front and back wheels independently, pan/tilt the camera mount, and view a live SJ4000 feed.

Built for **ESP32-C3** (LAN/Ethernet) + **SJCAM SJ4000** (Wi-Fi camera).

## Features

- **Robot connection** — ESP32-C3 over LAN
- **Dual-steer drive** — Independent front and back steering
- **Camera mount control** — Pan/tilt from the keyboard
- **SJ4000 live feed** — MJPEG or RTSP
- **ESP32 firmware** — Matches the UI command protocol

## Hardware

| Component | Role |
|-----------|------|
| ESP32-C3 + W5500 | Robot controller over Ethernet |
| Drive motors | Forward / reverse |
| Front & back steer servos | Independent wheel steering |
| Pan/tilt servos | Camera mount orientation |
| SJCAM SJ4000 | Wi-Fi video feed |
| PC | Runs the web UI |

## Network setup

```
PC ── Ethernet ──► Router/Switch ──► ESP32-C3 (192.168.1.100)
PC ── Wi-Fi ──────► SJ4000 AP (192.168.1.254)
```

| Device | Default IP |
|--------|------------|
| ESP32-C3 | `192.168.1.100` |
| SJ4000 | `192.168.1.254` |

## Quick start

```bash
pip install -r requirements.txt
python app.py
```

Open **http://127.0.0.1:5000**

1. Connect robot over LAN and click **Connect**
2. Join SJ4000 Wi-Fi and click **Start Feed**
3. Use the **Dual-Steer Robot Panel** or keyboard

## Controls

### Drive
| Key / Button | Action |
|--------------|--------|
| `W` | Forward (hold) |
| `S` | Backward (hold) |
| Stop | Stop drive |

Hold `W`/`S` to move. Release to stop.

### Front steering
| Key | Action |
|-----|--------|
| `A` | Steer left (hold) |
| `D` | Steer right (hold) |
| `F` | Re-center front wheels |

### Back steering
| Key | Action |
|-----|--------|
| `Z` | Steer left (hold) |
| `C` | Steer right (hold) |
| `V` | Re-center back wheels |

Hold steer keys to adjust angle. Release to hold the current angle.

### Camera mount
| Key | Action |
|-----|--------|
| `8` | Tilt up (hold) |
| `2` | Tilt down (hold) |
| `4` | Pan left (hold) |
| `6` | Pan right (hold) |
| `5` | Re-center camera |

Numpad keys are supported (`Numpad8`, etc.).

## ESP32-C3 firmware

```
firmware/pipeinspect_esp32c3/pipeinspect_esp32c3.ino
```

### HTTP API

| Method | Endpoint | Body |
|--------|----------|------|
| `GET` | `/status` | — |
| `POST` | `/command` | `{"key":"W","action":"down"}` |

**Actions:** `down` (key held), `up` (key released), `press` (momentary)

**Keys:** `W`, `S`, `STOP`, `A`, `D`, `F`, `Z`, `C`, `V`, `8`, `2`, `4`, `6`, `5`

Example response:

```json
{"key_received":"W","action":"up"}
```

Update motor pins, servo pins, and network settings in the sketch before flashing.

## SJ4000 camera

- **MJPEG (Photo mode):** `http://192.168.1.254:8192/`
- **RTSP (Video mode):** `rtsp://192.168.1.254/sjcam.mov` (requires FFmpeg on PC)

References: [SJCam API](https://github.com/Zsub/SJCam-API), [SJ4000 streaming notes](https://video.stackexchange.com/questions/19886/how-to-set-up-a-livestream-from-sj4000-wifi)

## Project structure

```
getabec/
├── app.py
├── requirements.txt
├── templates/index.html
├── static/css/style.css
├── static/js/app.js
└── firmware/pipeinspect_esp32c3/pipeinspect_esp32c3.ino
```

## Troubleshooting

| Problem | Check |
|---------|--------|
| Robot won't connect | Ethernet cable, same LAN, correct IP in firmware |
| Steer doesn't hold angle | ESP32 firmware flashed with dual-steer sketch |
| Camera feed fails | PC on SJ4000 Wi-Fi, correct camera mode |
| RTSP fails | Install FFmpeg, use Video mode on SJ4000 |
