/*
 * PipeInspect robot firmware for ESP32-C3 + W5500 Ethernet module.
 *
 * Matches the PipeInspect UI HTTP API:
 *   GET  /status   -> {"device":"ESP32-C3","key_received":"None"}
 *   POST /command  -> body: {"key":"W"}  (W, A, S, D, STOP or space)
 *
 * Libraries (Arduino Library Manager):
 *   - Ethernet (by Arduino, for W5500)
 *
 * Wiring depends on your board. Update the pin and network settings below.
 */

#include <SPI.h>
#include <Ethernet.h>

// --- Network (set to match your LAN router) ---
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// --- W5500 SPI pins (change for your ESP32-C3 board) ---
#define W5500_CS   7
#define W5500_RST  10
#define SPI_MOSI   6
#define SPI_MISO   5
#define SPI_SCK    4

// --- Motor driver pins (update for your robot) ---
#define MOTOR_LEFT_FWD   0
#define MOTOR_LEFT_REV   1
#define MOTOR_RIGHT_FWD  2
#define MOTOR_RIGHT_REV  3

EthernetServer server(80);

String lastKey = "None";

void stopMotors() {
  digitalWrite(MOTOR_LEFT_FWD, LOW);
  digitalWrite(MOTOR_LEFT_REV, LOW);
  digitalWrite(MOTOR_RIGHT_FWD, LOW);
  digitalWrite(MOTOR_RIGHT_REV, LOW);
}

void driveForward() {
  digitalWrite(MOTOR_LEFT_FWD, HIGH);
  digitalWrite(MOTOR_LEFT_REV, LOW);
  digitalWrite(MOTOR_RIGHT_FWD, HIGH);
  digitalWrite(MOTOR_RIGHT_REV, LOW);
}

void driveBackward() {
  digitalWrite(MOTOR_LEFT_FWD, LOW);
  digitalWrite(MOTOR_LEFT_REV, HIGH);
  digitalWrite(MOTOR_RIGHT_FWD, LOW);
  digitalWrite(MOTOR_RIGHT_REV, HIGH);
}

void turnLeft() {
  digitalWrite(MOTOR_LEFT_FWD, LOW);
  digitalWrite(MOTOR_LEFT_REV, HIGH);
  digitalWrite(MOTOR_RIGHT_FWD, HIGH);
  digitalWrite(MOTOR_RIGHT_REV, LOW);
}

void turnRight() {
  digitalWrite(MOTOR_LEFT_FWD, HIGH);
  digitalWrite(MOTOR_LEFT_REV, LOW);
  digitalWrite(MOTOR_RIGHT_FWD, LOW);
  digitalWrite(MOTOR_RIGHT_REV, HIGH);
}

void applyKey(const String &key) {
  lastKey = key;

  if (key == "W") {
    driveForward();
  } else if (key == "S") {
    driveBackward();
  } else if (key == "A") {
    turnLeft();
  } else if (key == "D") {
    turnRight();
  } else if (key == "STOP" || key == " ") {
    stopMotors();
    lastKey = "None";
  } else {
    stopMotors();
  }
}

String extractJsonKey(const String &body) {
  int marker = body.indexOf("\"key\"");
  if (marker < 0) {
    return "";
  }

  int colon = body.indexOf(':', marker);
  int quoteStart = body.indexOf('"', colon + 1);
  int quoteEnd = body.indexOf('"', quoteStart + 1);
  if (quoteStart < 0 || quoteEnd < 0) {
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
    String json = String("{\"device\":\"ESP32-C3\",\"key_received\":\"") + lastKey + "\"}";
    sendJson(client, json);
    return;
  }

  if (method == "GET" && path.startsWith("/command")) {
    int keyIndex = path.indexOf("key=");
    String key = keyIndex >= 0 ? path.substring(keyIndex + 4) : "";
    key.replace("%20", " ");
    applyKey(key);
    String json = String("{\"key_received\":\"") + lastKey + "\"}";
    sendJson(client, json);
    return;
  }

  if (method == "POST" && path == "/command") {
    String key = extractJsonKey(body);
    applyKey(key);
    String json = String("{\"key_received\":\"") + lastKey + "\"}";
    sendJson(client, json);
    return;
  }

  sendNotFound(client);
}

void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_LEFT_FWD, OUTPUT);
  pinMode(MOTOR_LEFT_REV, OUTPUT);
  pinMode(MOTOR_RIGHT_FWD, OUTPUT);
  pinMode(MOTOR_RIGHT_REV, OUTPUT);
  stopMotors();

  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, HIGH);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, W5500_CS);
  Ethernet.init(W5500_CS);
  Ethernet.begin(mac, ip, gateway, subnet);

  server.begin();

  Serial.print("PipeInspect ESP32-C3 ready at ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  EthernetClient client = server.available();
  if (client) {
    handleClient(client);
    delay(1);
    client.stop();
  }
}
