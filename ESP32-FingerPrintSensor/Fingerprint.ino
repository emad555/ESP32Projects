#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

const char* ssid = "ssid";
const char* password = "password";

// Web interface credentials
const char* www_username = "admin";  // Web interface username
const char* www_password = "123";  // Web interface password

WebServer server(80);
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

uint8_t getFingerprintEnroll(uint8_t id);
uint8_t getFingerprintID();
void handleRoot();
void handleEnroll();
void handleVerify();
void handleList();
void handleLogin();
void handleLoginPost();
bool is_authenticated();
char* base64_decode(const char* input);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 1000); // Wait up to 1s for Serial
  delay(100);
  Serial.println("\n\nESP32 Fingerprint Web App");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: ");
  Serial.println(WiFi.localIP());

  mySerial.begin(57600, SERIAL_8N1, 16, 17); // UART2: RX=16, TX=17
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  server.on("/", handleRoot);
  server.on("/login", HTTP_GET, handleLogin);
  server.on("/login", HTTP_POST, handleLoginPost);
  server.on("/enroll", HTTP_POST, handleEnroll);
  server.on("/verify", HTTP_GET, handleVerify);
  server.on("/list", HTTP_GET, handleList);
  server.begin();
}

void loop() {
  server.handleClient();
  delay(10);
}
bool authenticated = false;

// Replace your is_authenticated() function with this:
bool is_authenticated() {
  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      return true;
    }
  }
  return authenticated;
}


char* base64_decode(const char* input) {
  const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t input_len = strlen(input);
  char* decoded = (char*)malloc(input_len);
  if (decoded == NULL) return NULL;
  
  size_t decoded_len = 0;
  int buffer = 0;
  int bits = 0;
  
  for (size_t i = 0; i < input_len; i++) {
    char c = input[i];
    if (c == '=') break;
    
    const char* ptr = strchr(b64chars, c);
    if (ptr == NULL) continue;
    
    buffer = (buffer << 6) | (ptr - b64chars);
    bits += 6;
    
    if (bits >= 8) {
      bits -= 8;
      decoded[decoded_len++] = (buffer >> bits) & 0xFF;
    }
  }
  
  decoded[decoded_len] = '\0';
  return decoded;
}

void handleLogin() {
  String msg = "";
  if (server.hasArg("DISCONNECT")) {
    authenticated = false;
    msg = "Logged out";
  }
  
  String content = "<html><body>";
  content += "<form action='/login' method='POST'>";
  if (msg != "") {
    content += "<div style='color:red;'>" + msg + "</div>";
  }
  content += "Username: <input type='text' name='username'><br>";
  content += "Password: <input type='password' name='password'><br>";
  content += "<input type='submit' name='submit' value='Login'></form>";
  content += "</body></html>";
  
  server.send(200, "text/html", content);
}

String base64_encode(String input) {
  const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String output = "";
  int i = 0;
  int j = 0;
  uint8_t char_array_3[3];
  uint8_t char_array_4[4];
  
  while (input.length() > i) {
    char_array_3[j++] = input[i++];
    if (j == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;
      
      for (j = 0; j < 4; j++) {
        output += b64chars[char_array_4[j]];
      }
      j = 0;
    }
  }
  
  if (j > 0) {
    for (int i = j; i < 3; i++) {
      char_array_3[i] = '\0';
    }
    
    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    
    for (int i = 0; i < j + 1; i++) {
      output += b64chars[char_array_4[i]];
    }
    
    while (j++ < 3) {
      output += '=';
    }
  }
  
  return output;
}
// Modified handleLoginPost function
void handleLoginPost() {
  String username = server.arg("username");
  String password = server.arg("password");
  
  if (username == www_username && password == www_password) {
    authenticated = true;
    server.sendHeader("Location", "/");
    server.sendHeader("Set-Cookie", "ESPSESSIONID=1");
    server.send(301);
  } else {
    authenticated = false;
    String content = "<html><body><form action='/login' method='POST'>";
    content += "<div style='color:red;'>Invalid username or password!</div>";
    content += "Username: <input type='text' name='username'><br>";
    content += "Password: <input type='password' name='password'><br>";
    content += "<input type='submit' name='submit' value='Login'></form>";
    content += "</body></html>";
    server.send(200, "text/html", content);
  }
}
void handleRoot() {
  if (!is_authenticated()) {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }

  String html = R"(
    <!DOCTYPE html>
<html lang='en'>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Fingerprint Sensor</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background-color: #f0f2f5;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      margin: 0;
      position: relative; /* For absolute positioning of logout button */
    }
    .container {
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.1);
      max-width: 400px;
      width: 100%;
      text-align: center;
      position: relative;
    }
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
    }
    h1 {
      color: #333;
      margin: 0;
      font-size: 24px;
    }
    .section {
      margin: 20px 0;
    }
    input[type='number'] {
      padding: 10px;
      width: 100px;
      border: 1px solid #ccc;
      border-radius: 5px;
      margin-right: 10px;
    }
    button {
      padding: 10px 20px;
      background-color: #28a745;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      transition: background-color 0.3s;
      margin: 5px;
    }
    button:hover {
      background-color: #218838;
    }
    #status {
      margin-top: 20px;
      padding: 10px;
      background-color: #e9ecef;
      border-radius: 5px;
      min-height: 50px;
      color: #333;
    }
    pre {
      background-color: #f8f9fa;
      border: 1px solid #ddd;
      border-radius: 5px;
      padding: 10px;
      max-height: 200px;
      overflow-y: auto;
      text-align: left;
    }
    .btn-blue {
      background-color: #007bff;
    }
    .btn-blue:hover {
      background-color: #0069d9;
    }
    /* Logout Button Styles */
    .logout-btn {
      padding: 8px 16px;
      background-color: #dc3545;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      transition: all 0.3s;
      text-decoration: none;
      font-size: 14px;
      display: inline-block;
    }
    .logout-btn:hover {
      background-color: #c82333;
      transform: translateY(-1px);
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    }
  </style>
</head>
<body>
  <div class='container'>
    <div class='header'>
      <h1>Fingerprint Sensor</h1>
      <a href='/login?DISCONNECT=YES' class='logout-btn'>Logout</a>
    </div>
    
    <div class='section'>
      <h3>Enroll Fingerprint</h3>
      <form onsubmit='enrollFingerprint(event)'>
        <input type='number' id='enrollId' min='1' max='127' placeholder='ID (1-127)' required>
        <button type='submit'>Enroll</button>
      </form>
    </div>
    
    <div class='section'>
      <h3>Verify Fingerprint</h3>
      <button onclick='verifyFingerprint()'>Verify</button>
    </div>
    
    <div class='section'>
      <h3>Stored Fingerprints</h3>
      <button class='btn-blue' onclick='listFingerprints()'>List Stored</button>
      <pre id='fingerprintList'>No fingerprints loaded yet</pre>
    </div>
    
    <div class='section'>
      <h3>Status</h3>
      <div id='status'>Ready</div>
    </div>
  </div>

  <script>
    function enrollFingerprint(event) {
      event.preventDefault();
      const id = document.getElementById('enrollId').value;
      const status = document.getElementById('status');
      status.textContent = 'Enrolling fingerprint...';
      
      fetch('/enroll', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: 'id=' + id
      })
      .then(response => response.text())
      .then(data => {
        status.textContent = data;
        if(data === 'Fingerprint enrolled') {
          listFingerprints(); // Refresh the fingerprint list
        }
      })
      .catch(error => status.textContent = 'Error: ' + error.message);
    }
    
    function verifyFingerprint() {
      const status = document.getElementById('status');
      status.textContent = 'Waiting for finger...';
      fetch('/verify')
        .then(response => response.text())
        .then(data => status.textContent = data)
        .catch(error => status.textContent = 'Error: ' + error.message);
    }
    
    function listFingerprints() {
      const listElement = document.getElementById('fingerprintList');
      listElement.textContent = 'Loading...';
      fetch('/list')
        .then(response => response.text())
        .then(data => listElement.textContent = data)
        .catch(error => listElement.textContent = 'Error: ' + error.message);
    }
  </script>
</body>
</html>
  )";
  server.send(200, "text/html", html);
}

void handleEnroll() {
  if (!is_authenticated()) {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }

  if (server.hasArg("id")) {
    uint8_t id = server.arg("id").toInt();
    if (id < 1 || id > 127) {
      server.send(400, "text/plain", "Invalid ID (1-127)");
      return;
    }
    Serial.print("Enrolling ID #"); Serial.println(id);
    if (getFingerprintEnroll(id)) {
      server.send(200, "text/plain", "Fingerprint enrolled");
    } else {
      server.send(500, "text/plain", "Enrollment failed");
    }
  } else {
    server.send(400, "text/plain", "No ID provided");
  }
}

void handleVerify() {
  if (!is_authenticated()) {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }

  uint8_t result = getFingerprintID();
  if (result == FINGERPRINT_OK) {
    String msg = "Match found: ID #" + String(finger.fingerID);
    server.send(200, "text/plain", msg);
  } else if (result == FINGERPRINT_NOTFOUND) {
    server.send(200, "text/plain", "No match found");
  } else if (result == FINGERPRINT_NOFINGER) {
    server.send(408, "text/plain", "Timeout: No finger detected");
  } else {
    server.send(500, "text/plain", "Verification failed");
  }
}

void handleList() {
  if (!is_authenticated()) {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }

  String response = "Stored Fingerprint IDs:\n";
  for (int id = 1; id <= 127; id++) {
    if (finger.loadModel(id) == FINGERPRINT_OK) {
      response += String(id) + "\n";
    }
  }
  if (response == "Stored Fingerprint IDs:\n") {
    response += "No fingerprints stored";
  }
  server.send(200, "text/plain", response);
}

uint8_t getFingerprintEnroll(uint8_t id) {
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        return p;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        return p;
      default:
        Serial.println("Unknown error");
        return p;
    }
  }

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        return p;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        return p;
      default:
        Serial.println("Unknown error");
        return p;
    }
  }

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.print("Creating model for #"); Serial.println(id);
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}

uint8_t getFingerprintID() {
  uint8_t p = -1;
  Serial.println("Waiting for finger...");
  unsigned long startTime = millis();
  const unsigned long timeout = 10000; // Wait up to 10 seconds
  while (p != FINGERPRINT_OK && (millis() - startTime) < timeout) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        return p;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        return p;
      default:
        Serial.println("Unknown error");
        return p;
    }
    delay(100); // Prevent tight looping
  }

  if (p != FINGERPRINT_OK) {
    Serial.println("Timeout or error: No finger detected");
    return FINGERPRINT_NOFINGER;
  }

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("Found ID #"); Serial.print(finger.fingerID);
    Serial.print(" with confidence of "); Serial.println(finger.confidence);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }
  return p;
}
