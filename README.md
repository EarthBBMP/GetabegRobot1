# PipeInspect UI

Web control dashboard for a pipe inspection robot. Drive the robot over Ethernet, view a live SJ4000 camera feed, and send movement commands from the keyboard.

Built for **ESP32-C3** (LAN/Ethernet) + **SJCAM SJ4000** (Wi-Fi camera).

## Features

- **Robot connection** — Enter the ESP32-C3 IP and connect over LAN
- **WASD control** — Forward, reverse, turn left/right; Space to stop
- **SJ4000 live feed** — MJPEG (Photo mode) or RTSP (Video mode)
- **ESP32 firmware** — Ready-to-flash sketch with motor control stubs

## Hardware

| Component | Role |
|-----------|------|
| ESP32-C3 + W5500 | Robot controller over Ethernet |
| Motor driver + motors | Tank-drive movement |
| SJCAM SJ4000 | Wi-Fi camera on the robot |
| PC | Runs the web UI and connects to both robot and camera |

## Network setup

```
PC ── Ethernet ──► Router/Switch ──► ESP32-C3 (192.168.1.100)
PC ── Wi-Fi ──────► SJ4000 AP (192.168.1.254)
```

Your PC uses **Ethernet for the robot** and **Wi-Fi for the camera** at the same time.

Default IPs (change in firmware / UI if your network differs):

| Device | Default IP |
|--------|------------|
| ESP32-C3 | `192.168.1.100` |
| SJ4000 | `192.168.1.254` |

## Quick start

### 1. Install dependencies

```bash
pip install -r requirements.txt
```

### 2. Run the web UI

```bash
python app.py
```

Open **http://127.0.0.1:5000** in your browser.

### 3. Connect the robot

1. Plug the ESP32-C3 into your LAN with Ethernet.
2. Flash the firmware (see below).
3. Enter the robot IP in the UI and click **Connect**.

### 4. Start the camera feed

1. Turn on SJ4000 Wi-Fi and connect your PC to the camera network.
2. In the UI, choose a stream preset:
   - **SJ4000 MJPEG (Photo mode)** — recommended, no extra software
   - **SJ4000 RTSP (Video mode)** — requires [FFmpeg](https://ffmpeg.org/) on your PC
3. Click **Start Feed**.

## Controls

| Key | Action |
|-----|--------|
| `W` | Forward |
| `S` | Reverse |
| `A` | Turn left |
| `D` | Turn right |
| `Space` | Stop |

## ESP32-C3 firmware

Sketch location:

```
firmware/pipeinspect_esp32c3/pipeinspect_esp32c3.ino
```

### Flash steps

1. Open the sketch in Arduino IDE.
2. Install board support for **ESP32-C3**.
3. Install the **Ethernet** library (W5500).
4. Update these settings in the sketch for your hardware:
   - W5500 SPI pins (`W5500_CS`, `SPI_MOSI`, etc.)
   - Motor driver pins
   - Static IP, gateway, and subnet
5. Upload to the ESP32-C3.

### HTTP API (robot)

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/status` | Returns `{"device":"ESP32-C3","key_received":"None"}` |
| `POST` | `/command` | Body: `{"key":"W"}` — keys: `W`, `A`, `S`, `D`, ` ` (stop) |
| `GET` | `/command?key=W` | Alternative GET format |

## SJ4000 camera notes

- **MJPEG:** Put the camera in **Photo mode**. Stream URL: `http://192.168.1.254:8192/`
- **RTSP:** Put the camera in **Video mode**. Stream URL: `rtsp://192.168.1.254/sjcam.mov`
- The UI can switch camera mode automatically when you start the feed.

References: [SJCam API docs](https://github.com/Zsub/SJCam-API), [SJ4000 WiFi streaming discussion](https://video.stackexchange.com/questions/19886/how-to-set-up-a-livestream-from-sj4000-wifi)

## Project structure

```
getabec/
├── app.py                          # Flask server + camera/robot proxy
├── requirements.txt
├── templates/
│   └── index.html                  # Main UI
├── static/
│   ├── css/style.css
│   └── js/app.js
└── firmware/
    └── pipeinspect_esp32c3/
        └── pipeinspect_esp32c3.ino # ESP32-C3 robot firmware
```

## Troubleshooting

| Problem | Check |
|---------|--------|
| Robot won't connect | Ethernet cable, same LAN as PC, correct IP in firmware |
| Camera feed fails | PC joined SJ4000 Wi-Fi, camera in correct mode (Photo for MJPEG) |
| RTSP feed fails | Install FFmpeg and set camera to Video mode |
| Motors don't move | Motor pin wiring and driver power in the firmware sketch |

## License

MIT — use and modify for your inspection robot project.
