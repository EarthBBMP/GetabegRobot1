const robotIpInput = document.getElementById("robot-ip");
const robotConnectBtn = document.getElementById("robot-connect");
const robotStatusEl = document.getElementById("robot-status");
const keyPressedEl = document.getElementById("key-pressed");
const robotResponseEl = document.getElementById("robot-response");

const cameraIpInput = document.getElementById("camera-ip");
const streamPresetSelect = document.getElementById("stream-preset");
const cameraUrlInput = document.getElementById("camera-url");
const cameraConnectBtn = document.getElementById("camera-connect");
const cameraStreamEl = document.getElementById("camera-stream");
const cameraPlaceholderEl = document.getElementById("camera-placeholder");

const keyGrid = document.getElementById("key-grid");
const keyCaps = [...document.querySelectorAll(".key-cap")];

const MOVEMENT_KEYS = new Set(["W", "A", "S", "D", " "]);
const KEY_LABELS = {
  " ": "STOP",
};

const SJ4000_IP = "192.168.1.254";

let robotConnected = false;
let robotIp = "";
let activeKeys = new Set();
let snapshotTimer = null;

function setRobotStatus(connected, message) {
  robotConnected = connected;
  robotStatusEl.textContent = message || (connected ? "Status: Connected" : "Status: Disconnected");
  robotStatusEl.classList.toggle("connected", connected);
  robotStatusEl.classList.toggle("disconnected", !connected);
}

function updateKeyDisplay(key) {
  const label = KEY_LABELS[key] || key || "None";
  keyPressedEl.textContent = label;
  keyCaps.forEach((cap) => {
    cap.classList.toggle("active", cap.dataset.key === key);
  });
}

function setRobotResponse(payload) {
  robotResponseEl.textContent = JSON.stringify(payload);
}

async function connectRobot() {
  robotIp = robotIpInput.value.trim();
  if (!robotIp) {
    setRobotStatus(false, "Status: Enter robot IP");
    return;
  }

  robotConnectBtn.disabled = true;
  setRobotStatus(false, "Status: Connecting...");

  try {
    const response = await fetch("/api/robot/connect", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ip: robotIp }),
    });
    const data = await response.json();

    if (!response.ok || !data.ok) {
      throw new Error(data.error || "Connection failed");
    }

    setRobotStatus(true);
    setRobotResponse(data.status || { device: "ESP32-C3", key_received: "None" });
  } catch (error) {
    setRobotStatus(false, "Status: Disconnected");
    setRobotResponse({ error: error.message });
  } finally {
    robotConnectBtn.disabled = false;
  }
}

async function sendCommand(key) {
  if (!robotConnected || !robotIp) {
    setRobotResponse({ key_received: key, warning: "Robot not connected" });
    return;
  }

  try {
    const response = await fetch("/api/robot/command", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ip: robotIp, key }),
    });
    const data = await response.json();

    if (!response.ok || !data.ok) {
      throw new Error(data.error || "Command failed");
    }

    setRobotResponse(data.response || { key_received: key });
  } catch (error) {
    setRobotResponse({ key_received: key, error: error.message });
  }
}

function buildCameraUrl() {
  const preset = streamPresetSelect.value;
  if (preset === "custom") {
    return cameraUrlInput.value.trim();
  }

  const ip = cameraIpInput.value.trim() || SJ4000_IP;

  if (preset === "sj4000_rtsp") {
    return `rtsp://${ip}/sjcam.mov`;
  }

  return `http://${ip}:8192/`;
}

async function prepareSj4000Feed() {
  const preset = streamPresetSelect.value;
  if (preset === "custom") {
    return;
  }

  const ip = cameraIpInput.value.trim() || SJ4000_IP;
  try {
    await fetch("/api/camera/sj4000/prepare", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ip, preset }),
    });
  } catch {
    // Camera prep is best-effort; stream may still work if mode is already correct.
  }
}

function stopCameraFeed() {
  if (snapshotTimer) {
    clearInterval(snapshotTimer);
    snapshotTimer = null;
  }
  cameraStreamEl.hidden = true;
  cameraStreamEl.removeAttribute("src");
  cameraPlaceholderEl.hidden = false;
}

async function startCameraFeed() {
  stopCameraFeed();

  const sourceUrl = buildCameraUrl();
  if (!sourceUrl) {
    cameraPlaceholderEl.textContent = "Enter a camera IP or custom stream URL.";
    return;
  }

  cameraPlaceholderEl.textContent = "Starting SJ4000 feed...";
  cameraPlaceholderEl.hidden = false;

  await prepareSj4000Feed();

  const preset = streamPresetSelect.value;

  if (preset === "sj4000_rtsp") {
    cameraStreamEl.src = `/api/camera/rtsp-stream?url=${encodeURIComponent(sourceUrl)}&t=${Date.now()}`;
  } else {
    cameraStreamEl.src = `/api/camera/stream?url=${encodeURIComponent(sourceUrl)}&t=${Date.now()}`;
  }

  cameraStreamEl.hidden = false;
  cameraPlaceholderEl.hidden = true;

  cameraStreamEl.onerror = () => {
    cameraPlaceholderEl.textContent =
      preset === "sj4000_rtsp"
        ? "RTSP feed failed. Install FFmpeg and put SJ4000 in Video mode, or use MJPEG (Photo mode)."
        : "MJPEG feed failed. Join SJ4000 Wi-Fi, set Photo mode, and check IP 192.168.1.254.";
    cameraPlaceholderEl.hidden = false;
  };
}

function normalizeKey(event) {
  const key = event.key.length === 1 ? event.key.toUpperCase() : event.key;
  if (key === "SPACE" || key === "Spacebar") {
    return " ";
  }
  return key;
}

function handleKeyDown(event) {
  if (event.target.matches("input, select, textarea")) {
    return;
  }

  const key = normalizeKey(event);
  if (!MOVEMENT_KEYS.has(key)) {
    return;
  }

  event.preventDefault();
  if (activeKeys.has(key)) {
    return;
  }

  activeKeys.add(key);
  updateKeyDisplay(key);
  sendCommand(key);
}

function handleKeyUp(event) {
  const key = normalizeKey(event);
  if (!MOVEMENT_KEYS.has(key)) {
    return;
  }

  activeKeys.delete(key);
  if (activeKeys.size === 0) {
    updateKeyDisplay("");
    setRobotResponse({ key_received: "None" });
  } else {
    const lastKey = [...activeKeys].at(-1);
    updateKeyDisplay(lastKey);
  }
}

streamPresetSelect.addEventListener("change", () => {
  const isCustom = streamPresetSelect.value === "custom";
  cameraUrlInput.classList.toggle("hidden", !isCustom);
});

robotConnectBtn.addEventListener("click", connectRobot);
cameraConnectBtn.addEventListener("click", startCameraFeed);
keyGrid.addEventListener("keydown", handleKeyDown);
keyGrid.addEventListener("keyup", handleKeyUp);
document.addEventListener("keydown", handleKeyDown);
document.addEventListener("keyup", handleKeyUp);

keyCaps.forEach((cap) => {
  cap.addEventListener("mousedown", (event) => {
    event.preventDefault();
    keyGrid.focus();
    const key = cap.dataset.key;
    updateKeyDisplay(key);
    sendCommand(key);
  });
});

keyGrid.focus();
setRobotResponse({ key_received: "None" });
