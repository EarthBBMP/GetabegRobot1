"""Pipe inspection robot control server."""

from __future__ import annotations

import io
import shutil
import subprocess
from urllib.parse import urlparse

import requests
from flask import Flask, Response, jsonify, render_template, request

app = Flask(__name__)

ROBOT_TIMEOUT = 2.0
CAMERA_TIMEOUT = 5.0
SJ4000_DEFAULT_IP = "192.168.1.254"


def _robot_base_url(ip: str) -> str:
    ip = ip.strip()
    if ip.startswith("http://") or ip.startswith("https://"):
        return ip.rstrip("/")
    return f"http://{ip}"


def _sj4000_base_url(ip: str) -> str:
    ip = (ip or SJ4000_DEFAULT_IP).strip()
    if ip.startswith("http://") or ip.startswith("https://"):
        return ip.rstrip("/")
    return f"http://{ip}"


def _ffmpeg_path() -> str | None:
    return shutil.which("ffmpeg")


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/robot/connect", methods=["POST"])
def robot_connect():
    data = request.get_json(silent=True) or {}
    ip = (data.get("ip") or "").strip()
    if not ip:
        return jsonify({"ok": False, "error": "Robot IP is required."}), 400

    base = _robot_base_url(ip)
    try:
        response = requests.get(f"{base}/status", timeout=ROBOT_TIMEOUT)
        response.raise_for_status()
        payload = response.json() if response.content else {}
        return jsonify({"ok": True, "status": payload, "base_url": base})
    except requests.exceptions.JSONDecodeError:
        return jsonify(
            {
                "ok": True,
                "status": {"device": "ESP32-C3", "key_received": "None"},
                "base_url": base,
            }
        )
    except requests.RequestException as exc:
        return jsonify(
            {
                "ok": False,
                "error": f"Could not reach ESP32-C3 at {base}. Check Ethernet/LAN cable and IP.",
                "detail": str(exc),
            }
        ), 502


@app.route("/api/robot/command", methods=["POST"])
def robot_command():
    data = request.get_json(silent=True) or {}
    ip = (data.get("ip") or "").strip()
    key = (data.get("key") or "").strip().upper()
    action = (data.get("action") or "press").strip().lower()
    if key == "SPACE":
        key = "STOP"

    if not ip:
        return jsonify({"ok": False, "error": "Robot IP is required."}), 400
    if not key:
        return jsonify({"ok": False, "error": "Key is required."}), 400

    base = _robot_base_url(ip)
    payload = {"key": key, "action": action}
    try:
        response = requests.post(
            f"{base}/command",
            json=payload,
            timeout=ROBOT_TIMEOUT,
        )
        response.raise_for_status()
        body = response.json() if response.content else {"key_received": key, "action": action}
        return jsonify({"ok": True, "response": body})
    except requests.exceptions.JSONDecodeError:
        return jsonify({"ok": True, "response": {"key_received": key, "action": action}})
    except requests.RequestException:
        try:
            response = requests.get(
                f"{base}/command",
                params={"key": key, "action": action},
                timeout=ROBOT_TIMEOUT,
            )
            response.raise_for_status()
            body = response.json() if response.content else {"key_received": key, "action": action}
            return jsonify({"ok": True, "response": body})
        except requests.RequestException as exc:
            return jsonify({"ok": False, "error": str(exc)}), 502


@app.route("/api/camera/sj4000/prepare", methods=["POST"])
def sj4000_prepare():
    """Switch SJ4000 into the mode needed for the selected stream preset."""
    data = request.get_json(silent=True) or {}
    ip = (data.get("ip") or SJ4000_DEFAULT_IP).strip()
    preset = (data.get("preset") or "sj4000_mjpeg").strip()
    base = _sj4000_base_url(ip)

    # cmd=3001: switch capture mode. par=0 photo (MJPEG :8192), par=1 video (RTSP).
    mode = "0" if preset == "sj4000_mjpeg" else "1"
    try:
        requests.get(
            f"{base}/",
            params={"custom": "1", "cmd": "3001", "par": mode},
            timeout=CAMERA_TIMEOUT,
        )
        return jsonify({"ok": True, "mode": "photo" if mode == "0" else "video"})
    except requests.RequestException as exc:
        return jsonify({"ok": False, "error": str(exc)}), 502


@app.route("/api/camera/stream")
def camera_stream():
    """Proxy MJPEG/HTTP camera stream to avoid browser CORS issues."""
    stream_url = (request.args.get("url") or "").strip()
    if not stream_url:
        return jsonify({"ok": False, "error": "Camera stream URL is required."}), 400

    parsed = urlparse(stream_url)
    if parsed.scheme not in {"http", "https"}:
        return jsonify({"ok": False, "error": "Only http/https stream URLs are supported."}), 400

    try:
        upstream = requests.get(stream_url, stream=True, timeout=CAMERA_TIMEOUT)
        upstream.raise_for_status()
    except requests.RequestException as exc:
        return jsonify({"ok": False, "error": str(exc)}), 502

    content_type = upstream.headers.get(
        "Content-Type",
        "multipart/x-mixed-replace; boundary=arflebarfle",
    )

    def generate():
        try:
            for chunk in upstream.iter_content(chunk_size=8192):
                if chunk:
                    yield chunk
        finally:
            upstream.close()

    return Response(generate(), content_type=content_type)


@app.route("/api/camera/rtsp-stream")
def camera_rtsp_stream():
    """Convert SJ4000 RTSP stream to MJPEG for browser display."""
    stream_url = (request.args.get("url") or "").strip()
    if not stream_url:
        return jsonify({"ok": False, "error": "RTSP URL is required."}), 400

    parsed = urlparse(stream_url)
    if parsed.scheme != "rtsp":
        return jsonify({"ok": False, "error": "Only rtsp:// URLs are supported here."}), 400

    ffmpeg = _ffmpeg_path()
    if not ffmpeg:
        return jsonify(
            {
                "ok": False,
                "error": "FFmpeg not found. Install FFmpeg or use SJ4000 MJPEG (Photo mode).",
            }
        ), 503

    command = [
        ffmpeg,
        "-rtsp_transport",
        "tcp",
        "-i",
        stream_url,
        "-an",
        "-c:v",
        "mjpeg",
        "-f",
        "mpjpeg",
        "-q:v",
        "8",
        "pipe:1",
    ]

    try:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
    except OSError as exc:
        return jsonify({"ok": False, "error": str(exc)}), 500

    def generate():
        try:
            assert process.stdout is not None
            while True:
                chunk = process.stdout.read(8192)
                if not chunk:
                    break
                yield chunk
        finally:
            if process.poll() is None:
                process.kill()

    return Response(generate(), content_type="multipart/x-mixed-replace; boundary=--BoundaryString")


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True, threaded=True)
