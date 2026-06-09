/*
 * PipeInspect dual-steer robot firmware for ESP32-C3 + W5500 Ethernet.
 *
 * HTTP API (matches PipeInspect UI):
 *   GET  /status
 *   POST /command  {"key":"W","action":"down"|"up"|"press"}
 *
 * Keys:
 *   Drive:     W, S, STOP
 *   Front:     A, D (hold steer), F (re-center)
 *   Back:      Z, C (hold steer), V (re-center)
 *   Camera:    8, 2, 4, 6 (hold pan/tilt), 5 (re-center)
 */

#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

#define W5500_CS   7
#define W5500_RST  10
#define SPI_MOSI   6
#define SPI_MISO   5
#define SPI_SCK    4

// Drive motors
#define DRIVE_FWD_PIN  0
#define DRIVE_REV_PIN  1

// Front steer servo signal pin
#define FRONT_STEER_PIN 2
// Back steer servo signal pin
#define BACK_STEER_PIN  3
// Camera pan/tilt servos
#define CAM_PAN_PIN  4
#define CAM_TILT_PIN 5

EthernetServer server(80);

String lastKey = "None";
String lastAction = "none";

int driveState = 0;       // -1 reverse, 0 stop, 1 forward
int frontSteer = 90;      // degrees, 90 = center
int backSteer = 90;
int camPan = 90;
int camTilt = 90;

const int STEER_MIN = 45;
const int STEER_MAX = 135;
const int STEER_STEP = 2;
const int CAM_STEP = 2;

bool frontSteerActive = false;
bool backSteerActive = false;
bool camPanActive = false;
bool camTiltActive = false;

int frontSteerDelta = 0;
int backSteerDelta = 0;
int camPanDelta = 0;
int camTiltDelta = 0;

unsigned long lastMotionMs = 0;
const unsigned long MOTION_INTERVAL_MS = 40;

void stopDrive() {
  driveState = 0;
  digitalWrite(DRIVE_FWD_PIN, LOW);
  digitalWrite(DRIVE_REV_PIN, LOW);
}

void applyDrive() {
  if (driveState > 0) {
    digitalWrite(DRIVE_FWD_PIN, HIGH);
    digitalWrite(DRIVE_REV_PIN, LOW);
  } else if (driveState < 0) {
    digitalWrite(DRIVE_FWD_PIN, LOW);
    digitalWrite(DRIVE_REV_PIN, HIGH);
  } else {
    stopDrive();
  }
}

void writeServoAngle(int pin, int angle) {
  angle = constrain(angle, STEER_MIN, STEER_MAX);
  // TODO: replace with your servo library (ESP32Servo, PCA9685, etc.)
  analogWrite(pin, map(angle, 0, 180, 0, 255));
}

void centerFrontSteer() {
  frontSteer = 90;
  writeServoAngle(FRONT_STEER_PIN, frontSteer);
}

void centerBackSteer() {
  backSteer = 90;
  writeServoAngle(BACK_STEER_PIN, backSteer);
}

void centerCamera() {
  camPan = 90;
  camTilt = 90;
  writeServoAngle(CAM_PAN_PIN, camPan);
  writeServoAngle(CAM_TILT_PIN, camTilt);
}

void updateMotion() {
  unsigned long now = millis();
  if (now - lastMotionMs < MOTION_INTERVAL_MS) {
    return;
  }
  lastMotionMs = now;

  if (frontSteerActive) {
    frontSteer = constrain(frontSteer + frontSteerDelta, STEER_MIN, STEER_MAX);
    writeServoAngle(FRONT_STEER_PIN, frontSteer);
  }
  if (backSteerActive) {
    backSteer = constrain(backSteer + backSteerDelta, STEER_MIN, STEER_MAX);
    writeServoAngle(BACK_STEER_PIN, backSteer);
  }
  if (camPanActive) {
    camPan = constrain(camPan + camPanDelta, STEER_MIN, STEER_MAX);
    writeServoAngle(CAM_PAN_PIN, camPan);
  }
  if (camTiltActive) {
    camTilt = constrain(camTilt + camTiltDelta, STEER_MIN, STEER_MAX);
    writeServoAngle(CAM_TILT_PIN, camTilt);
  }
}

void applyKeyAction(const String &key, const String &action) {
  lastKey = key;
  lastAction = action;

  if (key == "STOP" && (action == "press" || action == "down")) {
    stopDrive();
    lastKey = "None";
    return;
  }

  if (key == "W") {
    if (action == "down") {
      driveState = 1;
    } else if (action == "up") {
      stopDrive();
      lastKey = "None";
    }
    applyDrive();
    return;
  }

  if (key == "S") {
    if (action == "down") {
      driveState = -1;
    } else if (action == "up") {
      stopDrive();
      lastKey = "None";
    }
    applyDrive();
    return;
  }

  if (key == "A") {
    frontSteerActive = action == "down";
    frontSteerDelta = frontSteerActive ? -STEER_STEP : 0;
    return;
  }

  if (key == "D") {
    frontSteerActive = action == "down";
    frontSteerDelta = frontSteerActive ? STEER_STEP : 0;
    return;
  }

  if (key == "F" && action == "press") {
    centerFrontSteer();
    return;
  }

  if (key == "Z") {
    backSteerActive = action == "down";
    backSteerDelta = backSteerActive ? -STEER_STEP : 0;
    return;
  }

  if (key == "C") {
    backSteerActive = action == "down";
    backSteerDelta = backSteerActive ? STEER_STEP : 0;
    return;
  }

  if (key == "V" && action == "press") {
    centerBackSteer();
    return;
  }

  if (key == "8") {
    camTiltActive = action == "down";
    camTiltDelta = camTiltActive ? -CAM_STEP : 0;
    return;
  }

  if (key == "2") {
    camTiltActive = action == "down";
    camTiltDelta = camTiltActive ? CAM_STEP : 0;
    return;
  }

  if (key == "4") {
    camPanActive = action == "down";
    camPanDelta = camPanActive ? -CAM_STEP : 0;
    return;
  }

  if (key == "6") {
    camPanActive = action == "down";
    camPanDelta = camPanActive ? CAM_STEP : 0;
    return;
  }

  if (key == "5" && action == "press") {
    centerCamera();
  }
}

String queryValue(const String &query, const String &name) {
  String token = name + "=";
  int start = query.indexOf(token);
  if (start < 0) {
    return "";
  }
  start += token.length();
  int end = query.indexOf('&', start);
  if (end < 0) {
    return query.substring(start);
  }
  return query.substring(start, end);
}

String extractJsonField(const String &body, const String &field) {
  String marker = "\"" + field + "\"";
  int index = body.indexOf(marker);
  if (index < 0) {
    return "";
  }

  int colon = body.indexOf(':', index);
  int quoteStart = body.indexOf('"', colon + 1);
  if (quoteStart < 0) {
    return "";
  }
  int quoteEnd = body.indexOf('"', quoteStart + 1);
  if (quoteEnd < 0) {
    return "";
  }
  return body.substring(quoteStart + 1, quoteEnd);
}

void sendJson(EthernetClient &client, const String &json) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println();
  client.println(json);
}

void sendNotFound(EthernetClient &client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println("{\"error\":\"not found\"}");
}

void handleClient(EthernetClient &client) {
  String requestLine = client.readStringUntil('\n');
  if (requestLine.length() == 0) {
    return;
  }

  String method = requestLine.substring(0, requestLine.indexOf(' '));
  String path = requestLine.substring(requestLine.indexOf(' ') + 1);
  path = path.substring(0, path.indexOf(' '));

  int contentLength = 0;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      break;
    }
    if (line.startsWith("Content-Length:")) {
      contentLength = line.substring(15).toInt();
    }
  }

  String body = "";
  while (body.length() < (unsigned int)contentLength && client.available()) {
    body += (char)client.read();
  }

  if (method == "GET" && path == "/status") {
    String json = String("{\"device\":\"ESP32-C3\",\"key_received\":\"") + lastKey +
                  "\",\"action\":\"" + lastAction + "\"}";
    sendJson(client, json);
    return;
  }

  if (method == "GET" && path.startsWith("/command")) {
    int queryStart = path.indexOf('?');
    String query = queryStart >= 0 ? path.substring(queryStart + 1) : "";
    String key = queryValue(query, "key");
    String action = queryValue(query, "action");
    if (action.length() == 0) {
      action = "press";
    }
    applyKeyAction(key, action);
    String json = String("{\"key_received\":\"") + lastKey + "\",\"action\":\"" + lastAction + "\"}";
    sendJson(client, json);
    return;
  }

  if (method == "POST" && path == "/command") {
    String key = extractJsonField(body, "key");
    String action = extractJsonField(body, "action");
    if (action.length() == 0) {
      action = "press";
    }
    applyKeyAction(key, action);
    String json = String("{\"key_received\":\"") + lastKey + "\",\"action\":\"" + lastAction + "\"}";
    sendJson(client, json);
    return;
  }

  sendNotFound(client);
}

void setup() {
  Serial.begin(115200);

  pinMode(DRIVE_FWD_PIN, OUTPUT);
  pinMode(DRIVE_REV_PIN, OUTPUT);
  stopDrive();

  centerFrontSteer();
  centerBackSteer();
  centerCamera();

  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, HIGH);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, W5500_CS);
  Ethernet.init(W5500_CS);
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();

  Serial.print("PipeInspect dual-steer robot ready at ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  updateMotion();

  EthernetClient client = server.available();
  if (client) {
    handleClient(client);
    delay(1);
    client.stop();
  }
}
