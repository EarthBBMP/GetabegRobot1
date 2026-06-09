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

const controlPanel = document.getElementById("control-panel");
const padButtons = [...document.querySelectorAll(".pad-btn")];

const HOLD_KEYS = new Set(["W", "S", "A", "D", "Z", "C", "8", "2", "4", "6"]);
const PRESS_KEYS = new Set(["F", "V", "5", "STOP"]);
const ALL_KEYS = new Set([...HOLD_KEYS, ...PRESS_KEYS]);

const KEY_ALIASES = {
  ARROWUP: "W",
  ARROWDOWN: "S",
  ARROWLEFT: "A",
  ARROWRIGHT: "D",
  NUMPAD8: "8",
  NUMPAD2: "2",
  NUMPAD4: "4",
  NUMPAD6: "6",
  NUMPAD5: "5",
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

function updateKeyDisplay() {
  if (activeKeys.size === 0) {
    keyPressedEl.textContent = "None";
  } else {
    keyPressedEl.textContent = [...activeKeys].join(", ");
  }

  padButtons.forEach((btn) => {
    btn.classList.toggle("active", activeKeys.has(btn.dataset.key));
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

async function sendCommand(key, action) {
  if (!robotConnected || !robotIp) {
    setRobotResponse({ key_received: key, action, warning: "Robot not connected" });
    return;
  }

  try {
    const response = await fetch("/api/robot/command", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ip: robotIp, key, action }),
    });
    const data = await response.json();

    if (!response.ok || !data.ok) {
      throw new Error(data.error || "Command failed");
    }

    setRobotResponse(data.response || { key_received: key, action });
  } catch (error) {
    setRobotResponse({ key_received: key, action, error: error.message });
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
    // Best-effort camera mode switch.
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
  let key = event.key.length === 1 ? event.key.toUpperCase() : event.key.toUpperCase();
  if (KEY_ALIASES[key]) {
    key = KEY_ALIASES[key];
  }
  if (/^[0-9]$/.test(key)) {
    return key;
  }
  return key;
}

function isEditableTarget(target) {
  return target.matches("input, select, textarea");
}

function handleKeyDown(event) {
  if (isEditableTarget(event.target)) {
    return;
  }

  const key = normalizeKey(event);
  if (!ALL_KEYS.has(key)) {
    return;
  }

  event.preventDefault();

  if (PRESS_KEYS.has(key)) {
    if (event.repeat) {
      return;
    }
    sendCommand(key, "press");
    return;
  }

  if (activeKeys.has(key)) {
    return;
  }

  activeKeys.add(key);
  updateKeyDisplay();
  sendCommand(key, "down");
}

function handleKeyUp(event) {
  if (isEditableTarget(event.target)) {
    return;
  }

  const key = normalizeKey(event);
  if (!HOLD_KEYS.has(key)) {
    return;
  }

  event.preventDefault();
  activeKeys.delete(key);
  updateKeyDisplay();
  sendCommand(key, "up");
}

function bindPadButton(btn) {
  const key = btn.dataset.key;
  const action = btn.dataset.action;

  if (action === "press") {
    btn.addEventListener("click", (event) => {
      event.preventDefault();
      controlPanel.focus();
      sendCommand(key, "press");
    });
    return;
  }

  btn.addEventListener("mousedown", (event) => {
    event.preventDefault();
    controlPanel.focus();
    if (activeKeys.has(key)) {
      return;
    }
    activeKeys.add(key);
    updateKeyDisplay();
    sendCommand(key, "down");
  });

  btn.addEventListener("mouseup", () => {
    activeKeys.delete(key);
    updateKeyDisplay();
    sendCommand(key, "up");
  });

  btn.addEventListener("mouseleave", () => {
    if (!activeKeys.has(key)) {
      return;
    }
    activeKeys.delete(key);
    updateKeyDisplay();
    sendCommand(key, "up");
  });
}

streamPresetSelect.addEventListener("change", () => {
  const isCustom = streamPresetSelect.value === "custom";
  cameraUrlInput.classList.toggle("hidden", !isCustom);
});

robotConnectBtn.addEventListener("click", connectRobot);
cameraConnectBtn.addEventListener("click", startCameraFeed);
controlPanel.addEventListener("keydown", handleKeyDown);
controlPanel.addEventListener("keyup", handleKeyUp);
document.addEventListener("keydown", handleKeyDown);
document.addEventListener("keyup", handleKeyUp);
padButtons.forEach(bindPadButton);

controlPanel.focus();
setRobotResponse({ key_received: "None" });
