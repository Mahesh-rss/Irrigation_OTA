#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <ModbusMaster.h>
#include <HTTPClient.h>
#include <Update.h>
#include <base64.h>
#include <Preferences.h>
#include <Arduino.h>
#include <vector>
#include <string>
#include <AES.h>
#include <Crypto.h>  // For SHA-256
#include <SHA256.h>
#include <ArduinoJson.h>

/* Relay Pins Related variables */
#define RELAY_12 32
#define RELAY_14 33
const int flowMeterPin = 13;  // Pin where the flow meter is connected

volatile int pulseCount = 0;
float calibrationFactor;
float flowRate = 0.0;
unsigned long oldTime = 0;
bool bPumpStatus = false;
unsigned long previousMillis = 0;
const long signalPublishInterval = 10000;  // 10 seconds

/* Wifi Related variables */
String ssid;
String password;

double cumulativeWaterDischarge = 0.00;

/* MQTT Related variables */
#define SerialMon Serial
String MAC_ID;
const char* topicsub2 = "Irrigation123";

/* EEPROM Related variables */
#define EEPROM_SIZE 512  // Total EEPROM size

/* AP-Mode Related Variables */
int statusCode;
String st;
String content;
//Function Decalration
bool testWifi(void);
void launchWeb(void);
void setupAP(void);
//Establishing Local server at port 80
WebServer server(80);

String mqttServer = "";
int mqttPort;
String mqttUserName = "ttbs";
String mqttPassword = "ttbs@123";

WiFiClient client;
PubSubClient mqtt(client);

ModbusMaster node;
float wattstotal, disp_pf_avg, vatotal, varphase, vayphase, vabphase, vllavg, vryphase, vybphase, vbrphase, vlnavg, vrphase, vyphase, vbphase, iavg, irphase, iyphase, ibphase, freq, iewh, ievah, ilh, co2, tpfavg;

/*Version URL*/
//const char* version_url = "https://raw.githubusercontent.com/NandeepYadav/Test_OTA/main/version";
const char* version_url = "https://raw.githubusercontent.com/Mahesh-rss/Irrigation_OTA/main/version.txt";
/*Firmware URL*/
//const char* firmware_url = "https://raw.githubusercontent.com/NandeepYadav/Test_OTA/main/Irrigation_WiFi.ino.bin";
const char* firmware_url = "https://raw.githubusercontent.com/Mahesh-rss/Irrigation_OTA/main/build/esp32.esp32.esp32/irrigation_updated_code.ino.bin";
// Current firmware version
const String currentVersion = "2.5";

Preferences preferences;

String encryptionKey = "qtKgwYMEsukW2bqUtGZd6eKJPxtGtN1Fn13xS0gb6DhSi0WPdBsjCh973d8eqSk";

// Hash the key to get a 32-byte key for AES-256
byte hashedKey[32];
// hashKey(encryptionKey.c_str(), hashedKey, encryptionKey.length());

// Define the IV (must be 16 bytes)
byte iv[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

// Reset IV for decryption
byte ivDecrypt[16];
// memcpy(ivDecrypt, iv, 16);


void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  delay(10);

  hashKey(encryptionKey.c_str(), hashedKey, encryptionKey.length());
  memcpy(ivDecrypt, iv, 16);

  preferences.begin("storage", false);  // Open Preferences namespace

  Serial2.begin(9600, SERIAL_8E1);  // Use Serial1 for RS485 communication
  delay(100);
  node.begin(Serial2);

  pinMode(RELAY_12, OUTPUT);
  pinMode(RELAY_14, OUTPUT);
  pinMode(flowMeterPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(flowMeterPin), pulseCounter, RISING);
  oldTime = millis();

  // Initialize EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialize EEPROM");
    return;
  }

  ssid = String(readEEPROM(200, 239));
  Serial.println("WiFi SSID : " + String(ssid));

  password = String(readEEPROM(240, 279));
  Serial.println("WiFi PASSWORD : " + String(password));

  mqttServer = String(readEEPROM(280, 319));
  Serial.println("MQTT SERVER : " + String(mqttServer));

  mqttPort = String(readEEPROM(320, 329)).toInt();
  Serial.println("PORT : " + String(mqttPort));

  WiFi.mode(WIFI_AP_STA);

  launchWeb();
  setupAP();  // Setup HotSpot
  delay(100);
  server.handleClient();
  // Manually clear EEPROM (write 0 to all locations)
  // clearEEPROM();
  // EEPROM.commit();
  // Serial.println("EEPROM cleared...!");

  connectAndSubscribe();

  preferences.putInt("VRU_FROM", 0);
  preferences.putInt("VRU_TO", 9);
  preferences.putInt("VYU_FROM", 10);
  preferences.putInt("VYU_TO", 19);
  preferences.putInt("VBU_FROM", 20);
  preferences.putInt("VBU_TO", 29);
  preferences.putInt("RYU_FROM", 30);
  preferences.putInt("RYU_TO", 39);
  preferences.putInt("YBU_FROM", 40);
  preferences.putInt("YBU_TO", 49);
  preferences.putInt("BRU_FROM", 50);
  preferences.putInt("BRU_TO", 59);
  preferences.putInt("VRL_FROM", 60);
  preferences.putInt("VRL_TO", 69);
  preferences.putInt("VYL_FROM", 70);
  preferences.putInt("VYL_TO", 79);
  preferences.putInt("VBL_FROM", 80);
  preferences.putInt("VBL_TO", 89);
  preferences.putInt("RYL_FROM", 90);
  preferences.putInt("RYL_TO", 99);
  preferences.putInt("YBL_FROM", 100);
  preferences.putInt("YBL_TO", 109);
  preferences.putInt("BRL_FROM", 110);
  preferences.putInt("BRL_TO", 119);
  preferences.putInt("CR_FROM", 120);
  preferences.putInt("CR_TO", 129);
  preferences.putInt("CY_FROM", 130);
  preferences.putInt("CY_TO", 139);
  preferences.putInt("CB_FROM", 140);
  preferences.putInt("CB_TO", 149);
  preferences.putInt("CF_FROM", 150);
  preferences.putInt("CF_TO", 159);
  preferences.putInt("SSID_FROM", 200);
  preferences.putInt("SSID_TO", 239);
  preferences.putInt("PASS_FROM", 240);
  preferences.putInt("PASS_TO", 279);
  preferences.putInt("SER_FROM", 280);
  preferences.putInt("SER_TO", 319);
  preferences.putInt("POR_FROM", 320);
  preferences.putInt("POR_TO", 329);

  preferences.putString("VRU", readEEPROM(0, 9));
  preferences.putString("VYU", readEEPROM(10, 19));
  preferences.putString("VBU", readEEPROM(20, 29));
  preferences.putString("RYU", readEEPROM(30, 39));
  preferences.putString("YBU", readEEPROM(40, 49));
  preferences.putString("BRU", readEEPROM(50, 59));
  preferences.putString("VRL", readEEPROM(60, 69));
  preferences.putString("VYL", readEEPROM(70, 79));
  preferences.putString("VBL", readEEPROM(80, 89));
  preferences.putString("RYL", readEEPROM(90, 99));
  preferences.putString("YBL", readEEPROM(100, 109));
  preferences.putString("BRL", readEEPROM(110, 119));
  preferences.putString("CR", readEEPROM(120, 129));
  preferences.putString("CY", readEEPROM(130, 139));
  preferences.putString("CB", readEEPROM(140, 149));
  preferences.putString("CF", readEEPROM(150, 159));
  preferences.putString("SSID", readEEPROM(200, 239));
  preferences.putString("PASS", readEEPROM(240, 279));
  preferences.putString("SER", readEEPROM(280, 319));
  preferences.putString("POR", readEEPROM(320, 329));

  calibrationFactor = preferences.getString("CF").toFloat();
}

void loop() {
  // Serial.println("Current Firmware Version : " + String(currentVersion));
  server.handleClient();
  readAndPublishParams();
  // GetWaterDischarge();
  fetchAndPublishSignalStrength();

  if (WiFi.status() == WL_CONNECTED) {
    /* Loop the MQTT to keep Subscription active. */
    if (mqtt.connected()) {
      // Serial.println("=== MQTT CONNECTED ===");
      mqtt.loop();
    } else {
      Serial.println("=== MQTT DISCONNECTED ===");
      mqttReconnect();
    }
  } else {
    Serial.println("Wifi Connection Status : Disconnected");
    connectAndSubscribe();
  }
  delay(1000);
}

String initiateEncryption(String plainText) {
  return encryptString(plainText, hashedKey, iv);
}
String initiateDecryption(String encryptedText) {
  return decryptString(encryptedText, hashedKey, ivDecrypt);
}

void connectAndSubscribe() {
  server.handleClient();
  // Connect to WiFi
  WiFi.begin(ssid.c_str(), password.c_str());
  int nthAttempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    server.handleClient();
    delay(1000);
    Serial.println("Connecting to WiFi...");
    Serial.println("Connecting...");
    nthAttempt++;
  }
  server.handleClient();
  String macId = WiFi.macAddress();
  MAC_ID = macId;
  Serial.println("Mac-Id => " + String(macId));

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  Serial.println("Connected to WiFi");

  // Set up MQTT server
  mqtt.setServer(mqttServer.c_str(), mqttPort);
  mqtt.setCallback(mqttCallback);

  // Connect to MQTT broker
  while (!mqtt.connected()) {
    server.handleClient();
    Serial.println("Connecting to MQTT...");
    if (mqtt.connect(MAC_ID.c_str(), mqttUserName.c_str(), mqttPassword.c_str())) {
      Serial.println("Connected to MQTT broker");
      mqtt.subscribe(MAC_ID.c_str());
      mqtt.subscribe(topicsub2);
      mqtt.subscribe(String(MAC_ID + "--D").c_str());
    } else {
      Serial.print("Failed MQTT connection, rc=");
      Serial.print(mqtt.state());
      Serial.println(". Retrying in 5 seconds...");
      delay(1000);
      server.handleClient();
      delay(1000);
      server.handleClient();
      delay(1000);
      server.handleClient();
      delay(1000);
      server.handleClient();
      delay(1000);
    }
  }

  // Check and Update Firmware via OTA
  if (checkForUpdate()) {
    performOTA();
  } else {
    Serial.println("Already running the latest firmware.");
  }
}

void mqttReconnect() {
  while (!mqtt.connected() && WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    Serial.println("Connecting to MQTT...");
    if (mqtt.connect(MAC_ID.c_str(), mqttUserName.c_str(), mqttPassword.c_str())) {
      Serial.println("Connected to MQTT broker");
      mqtt.subscribe(MAC_ID.c_str());
      mqtt.subscribe(topicsub2);
      mqtt.subscribe(String(MAC_ID + "--D").c_str());
    } else {
      Serial.print("Failed MQTT connection, rc=");
      Serial.print(mqtt.state());
      Serial.println(". Retrying in 5 seconds...");
      delay(1000);
      server.handleClient();
      delay(1000);
      server.handleClient();
      delay(1000);
      server.handleClient();
      delay(1000);
      server.handleClient();
      delay(1000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String message;
  for (int i = 0; i < len; i++) {
    message += (char)payload[i];
  }

  Serial.println("----------------------------------------");
  SerialMon.println("***** Message Arrived *****");
  SerialMon.println("Topic : " + String(topic));
  SerialMon.println("Host : " + String(mqttServer) + ":" + String(mqttPort));
  SerialMon.println("Message Content : " + String(message));
  String messageArrivedTopic = String(topic);
  String mac_id_debug = String(MAC_ID + "--D");
  bool encryptionNeeded = !messageArrivedTopic.equals(mac_id_debug);
  /* Here macid--D topic sends data without encryption. So, we dont need to decrypt message came from that topic. */
  if (!messageArrivedTopic.equals(mac_id_debug)) {
    message = initiateDecryption(message);
    SerialMon.println("Decrypted Message Content : " + String(message));
  }
  message = String(parseJSON(message));
  SerialMon.println("Parsed JSON Message msg Content : " + String(message));
  Serial.println("----------------------------------------");
  Serial.println();


  if (message.equals("FETCH_EEPROM_DATA") && !String(topic).equals(topicsub2)) {
    Serial.println("----- :: Reading EEPROM :: ----- ");
    initiateEEPROMread(encryptionNeeded);
    Serial.println();
    return;
  } else if (message.equals("RESET_CONTROLLER")) {
    Serial.println(" ***** Resetting the MicroController..! ***** ");
    ESP.restart();
  } else if (message.equals("ON")) {
    Serial.println(" ***** Turning ON the MicroController ***** ");
    bPumpStatus = true;
    digitalWrite(RELAY_12, HIGH);
    delay(3000);
    digitalWrite(RELAY_12, LOW);
    delay(1000);
    return;
  } else if (message.equals("OFF")) {
    Serial.println(" ***** Turning OFF the MicroController ***** ");
    cumulativeWaterDischarge = 0.00;
    bPumpStatus = false;
    digitalWrite(RELAY_14, HIGH);
    delay(3000);
    digitalWrite(RELAY_14, LOW);
    delay(1000);
    return;
  } else if (message.equals("VERSION_CHECK") && !String(topic).equals(topicsub2)) {
    DynamicJsonDocument versionDoc(1024);
    versionDoc["id"] = String(MAC_ID);
    versionDoc["info"] = "Version";
    versionDoc["version"] = String(currentVersion);
    String jsonBuffer;
    serializeJson(versionDoc, jsonBuffer);
    String versionData = encryptionNeeded ? initiateEncryption(String(jsonBuffer)) : String(jsonBuffer);
    mqtt.publish("Status", versionData.c_str());
    return;
  } else if (message.equals("Status") && !String(topic).equals(topicsub2)) {
    DynamicJsonDocument statusDoc(1024);
    statusDoc["id"] = String(MAC_ID);
    statusDoc["info"] = "Status";
    statusDoc["status"] = "ACTIVE";
    String jsonBuffer;
    serializeJson(statusDoc, jsonBuffer);
    String statusData = encryptionNeeded ? initiateEncryption(String(jsonBuffer)) : String(jsonBuffer);
    mqtt.publish("Status", statusData.c_str());
    return;
  }
  // initiateEEPROMwrite(message);
  parseData(message);
}

String parseJSON(String jsonString) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.f_str());
    return "ERROR PARSING JSON..";
  }
  // const char* msg = doc["msg"];
  String msg = doc["msg"];
  Serial.print("Extracted message: ");
  Serial.println(msg);
  return String(msg);
}

void parseData(String input) {
  std::string data = input.c_str();                                // Input data
  std::vector<std::pair<std::string, std::string>> keyValuePairs;  // Dynamic storage

  size_t start = 0, end;
  while ((end = data.find(',', start)) != std::string::npos) {
    std::string pair = data.substr(start, end - start);
    size_t separator = pair.find(':');
    if (separator != std::string::npos) {
      keyValuePairs.emplace_back(pair.substr(0, separator), pair.substr(separator + 1));
    }
    start = end + 1;
  }

  // Handle the last key-value pair
  size_t separator = data.find(':', start);
  if (separator != std::string::npos) {
    keyValuePairs.emplace_back(data.substr(start, separator - start), data.substr(separator + 1));
  }

  // Print parsed key-value pairs
  Serial.println("Parsed Key-Value Pairs:");
  for (const auto& pair : keyValuePairs) {
    String key = String(pair.first.c_str());
    key.trim();
    String value = String(pair.second.c_str());
    value.trim();
    Serial.println(key + " : " + value);
    initiateEEPROMwrite(key, value);
  }
}

void initiateEEPROMwrite(String key, String value) {
  String msgKey = String(key);
  if (!msgKey.equals("CF") && !msgKey.equals("VRU") && !msgKey.equals("VYU")
      && !msgKey.equals("VBU") && !msgKey.equals("RYU") && !msgKey.equals("YBU")
      && !msgKey.equals("BRU") && !msgKey.equals("VRL") && !msgKey.equals("VYL")
      && !msgKey.equals("VBL") && !msgKey.equals("RYL") && !msgKey.equals("YBL")
      && !msgKey.equals("BRL") && !msgKey.equals("CR") && !msgKey.equals("CY")
      && !msgKey.equals("CB") && !msgKey.equals("SSID") && !msgKey.equals("PASS")
      && !msgKey.equals("SER") && !msgKey.equals("POR")) {
    Serial.println("ERROR.");
    return;
  }

  int from = preferences.getInt(String(String(key) + "_FROM").c_str(), 0);  // Default to 0 if not found
  int to = preferences.getInt(String(String(key) + "_TO").c_str(), 0);      // Default to 0 if not found
  Serial.println("From : " + String(from) + ", " + "To : " + String(to));
  preferences.putString(String(key).c_str(), String(value));
  writeEEPROM(from, to, String(value));
}

void initiateEEPROMread(bool encryptionNeeded) {
  Serial.println("************ Threshold-Data ************");
  DynamicJsonDocument thresholds(1024);
  thresholds["id"] = String(MAC_ID);
  thresholds["info"] = "Thresholds";
  thresholds["VRU"] = String(preferences.getString("VRU"));
  thresholds["VYU"] = String(preferences.getString("VYU"));
  thresholds["VBU"] = String(preferences.getString("VBU"));
  thresholds["RYU"] = String(preferences.getString("RYU"));
  thresholds["YBU"] = String(preferences.getString("YBU"));
  thresholds["BRU"] = String(preferences.getString("BRU"));
  thresholds["VRL"] = String(preferences.getString("VRL"));
  thresholds["VYL"] = String(preferences.getString("VYL"));
  thresholds["VBL"] = String(preferences.getString("VBL"));
  thresholds["RYL"] = String(preferences.getString("RYL"));
  thresholds["YBL"] = String(preferences.getString("YBL"));
  thresholds["BRL"] = String(preferences.getString("BRL"));
  thresholds["CR"] = String(preferences.getString("CR"));
  thresholds["CY"] = String(preferences.getString("CY"));
  thresholds["CB"] = String(preferences.getString("CB"));
  thresholds["CF"] = String(preferences.getString("CF"));
  thresholds["SER"] = String(preferences.getString("SER"));
  thresholds["POR"] = String(preferences.getString("POR"));
  Serial.println("*****************************************");
  String jsonString;
  serializeJson(thresholds, jsonString);
  // Publish the data
  if (mqtt.connected()) {
    Serial.println("Published Thresholds: " + String(jsonString));
    String thresholdData = encryptionNeeded ? initiateEncryption(String(jsonString)) : String(jsonString);
    if (mqtt.publish("Status", thresholdData.c_str())) {
      Serial.println("Thresholds published successfully.");
    } else {
      Serial.println("Error publishing Thresholds.");
    }
  }
}

// Function to write new data to EEPROM within a specified range
void writeEEPROM(int startAddress, int endAddress, String data) {
  int dataLength = data.length();

  if (dataLength > (endAddress - startAddress + 1)) {
    Serial.println("Error: Data is too long for the specified EEPROM range.");
    return;
  }

  // First, clear the EEPROM range
  clearEEPROMAt(startAddress, endAddress);

  // Write new data
  for (int i = 0; i < dataLength; i++) {
    EEPROM.write(startAddress + i, data[i]);
  }
  EEPROM.write(startAddress + dataLength, '\0');  // Null-terminate the string
  EEPROM.commit();                                // Save changes
  Serial.println("Data written to EEPROM successfully.");
}

// Function to read and print stored data from EEPROM
String readEEPROM(int startAddress, int endAddress) {
  String data = "";
  for (int i = startAddress; i <= endAddress; i++) {
    char value = EEPROM.read(i);
    if (value == '\0' || value == 0xFF) {  // Stop if null or empty data
      break;
    }
    // Serial.print(value);
    data += String(value);
  }
  return data;
}

// Function to clear EEPROM data from startAddress to endAddress
void clearEEPROMAt(int startAddress, int endAddress) {
  for (int i = startAddress; i <= endAddress; i++) {
    EEPROM.write(i, 0xFF);  // Write 0xFF to indicate erased state
  }
  EEPROM.commit();  // Save changes
  Serial.println("EEPROM cleared from " + String(startAddress) + " to " + String(endAddress) + ".");
}

// Function to clear EEPROM
void clearEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);  // Write 0 to all EEPROM addresses
  }
}

/********************** Fuctions used for WiFi credentials saving and connecting to it which you do not need to change **********************/
bool testWifi(void) {
  int c = 0;
  //Serial.println("Waiting for Wifi to connect");
  while (c < 20) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}

void launchWeb() {
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}

void setupAP(void) {
  // WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      //Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i) {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);

    st += ")";
    //st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  WiFi.begin();
  delay(100);
  String macid = WiFi.macAddress();
  Serial.print("MAC-ID : ");
  Serial.println(WiFi.macAddress());
  WiFi.softAP(String("Irrigation_" + macid.substring(macid.length() - 5)).c_str(), "");
  Serial.println("Initializing_softap_for_wifi credentials_modification");
  launchWeb();
  Serial.println("over");
}

void createWebServer() {
  {
    server.on("/", []() {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = ip.toString();

      content = "<!DOCTYPE HTML>\r\n<html>";
      content += "<head><style>";
      content += "body {font-family: Arial, sans-serif; background-color: #f4f4f9; margin: 0; padding: 0; color: #333;}";
      content += ".container {max-width: 500px; margin: 50px auto; padding: 20px; background-color: white; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);}";
      content += "h1 {color: #4CAF50; margin-bottom: 20px; text-align: center;}";
      content += ".ip-info {text-align: center; font-size: 18px; font-weight: bold; margin-bottom: 20px;}";
      content += "form {display: flex; flex-direction: column; align-items: center;}";
      content += ".form-group {display: flex; align-items: center; justify-content: flex-start; margin-bottom: 15px; width: 100%;}";
      content += ".form-group label {width: 100px; font-weight: bold; text-align: right;}";
      content += ".form-group span {margin: 0 10px; font-weight: bold;}";
      content += ".form-group input {flex-grow: 1; padding: 10px; border: 1px solid #ccc; border-radius: 4px;}";
      content += "input[type='submit'] {background-color: #4CAF50; color: white; padding: 8px 15px; border: none; border-radius: 4px; cursor: pointer; width: auto; font-size: 14px;}";
      content += "input[type='submit']:hover {background-color: #45a049;}";
      content += "</style></head>";

      content += "<body>";
      content += "<div class='container'>";
      content += "<h1>WiFi Credentials</h1>";
      // content += "<p class='ip-info'>Device IP Address: " + ipStr + "</p>";
      content += "<form method='post' action='/setting'>";

      content += "<div class='form-group'><label for='ssid'>SSID</label><span>:</span><input type='text' name='ssid' placeholder='Enter SSID' required></div>";
      content += "<div class='form-group'><label for='pass'>Password</label><span>:</span><input type='text' name='pass' placeholder='Enter Password' required></div>";
      content += "<div class='form-group'><label for='broker'>Server</label><span>:</span><input type='text' name='broker' placeholder='Enter Server Address' required></div>";
      content += "<div class='form-group'><label for='port'>Port</label><span>:</span><input type='text' name='port' placeholder='Enter Port' required></div>";

      content += "<input type='submit' value='Save Settings'>";
      content += "</form>";
      content += "</div>";
      content += "</body></html>";

      server.send(200, "text/html", content);
    });

    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      qsid.trim();
      String qpass = server.arg("pass");
      qpass.trim();
      String qserv = server.arg("broker");
      qserv.trim();
      String qport = server.arg("port");
      qport.trim();

      if (qsid.length() > 0 && qpass.length() > 0 && qserv.length() > 0 && qport.length() > 0) {

        Serial.println(qsid);
        Serial.println(qpass);
        Serial.println(qserv);
        Serial.println(qport);
        WiFi.begin(qsid.c_str(), qpass.c_str());

        int attempts = 0;

        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
          delay(500);
          attempts++;
        }

        if (WiFi.status() != WL_CONNECTED) {
          content = "<!DOCTYPE HTML><html><body style='font-family:Arial;text-align:center;padding-top:80px;'>";
          content += "<h2 style='color:#e53935;'>WiFi Connection Failed!</h2>";
          content += "<p>Please check SSID and Password.</p>";
          content += "</body></html>";

          server.send(400, "text/html", content);
          return;
        }

        Serial.println("writing eeprom ssid:");
        preferences.putString("SSID", qsid);
        writeEEPROM(200, 239, qsid);

        Serial.println("writing eeprom pass:");
        preferences.putString("PASS", qpass);
        writeEEPROM(240, 279, qpass);

        Serial.println("writing eeprom server:");
        preferences.putString("SER", qserv);
        writeEEPROM(280, 319, qserv);

        Serial.println("writing eeprom port:");
        preferences.putString("POR", qport);
        writeEEPROM(320, 329, qport);

        content = "<!DOCTYPE HTML><html>";
        content += "<head>";
        content += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        content += "<style>";
        content += "body{font-family:Arial;text-align:center;background:#f4f4f9;padding-top:80px;}";
        content += ".box{background:white;padding:30px;margin:auto;max-width:400px;border-radius:10px;box-shadow:0 4px 8px rgba(0,0,0,0.15);}";
        content += "h2{color:#4CAF50;}";
        content += "</style>";
        content += "</head>";
        content += "<body>";
        content += "<div class='box'>";
        content += "<h2>Success!</h2>";
        content += "<p>Settings saved successfully.</p>";
        content += "<p>Device restarting...</p>";
        content += "</div>";
        content += "</body></html>";

        statusCode = 200;

      } else {

        content = "<!DOCTYPE HTML><html>";
        content += "<head>";
        content += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        content += "<style>";
        content += "body{font-family:Arial;text-align:center;background:#f4f4f9;padding-top:80px;}";
        content += ".box{background:white;padding:30px;margin:auto;max-width:400px;border-radius:10px;}";
        content += "h2{color:#e53935;}";
        content += "</style>";
        content += "</head>";
        content += "<body>";
        content += "<div class='box'>";
        content += "<h2>Error!</h2>";
        content += "<p>Please fill all the fields.</p>";
        content += "</div>";
        content += "</body></html>";

        statusCode = 400;
      }

      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "text/html", content);

      if (statusCode == 200) {
        delay(5000);
        ESP.restart();
      }
    });
  }
}


// Function to Read Parameters from RS485
String readAndPublishParams() {

  uint16_t answer = 0;
  uint16_t DATA1[10];

  float currentAverage = 0.00;

  answer = node.readHoldingRegisters(2, 148, 6);
  if (answer == node.ku8MBSuccess) {
    for (int Aj = 0; Aj < 6; Aj++) {
      DATA1[Aj] = node.getResponseBuffer(Aj);
    }
    uint32_t combined = DATA1[1];
    combined = (combined << 16) | DATA1[0];
    memcpy(&iavg, &combined, 4);
    // Serial.print("Current Average :  ");
    // Serial.print(iavg, 2);
    currentAverage = String(iavg, 2).toFloat();
    // Serial.println();
  } else {
    Serial.println("Current Average Parameter Read Failure...!!!");
    server.handleClient();
    loopMqtt();
    return "";
  }
  delay(200);
  server.handleClient();
  loopMqtt();

  // currentAverage = 20.00;

  if (currentAverage > 0) {

    String wtrData = String(GetWaterDischarge());

    DynamicJsonDocument docu(1024);
    docu["wtr"] = wtrData;
    docu["id"] = String(MAC_ID);
    docu["info"] = "Readings";

    String strValues = "";

    //  answer = node.readHoldingRegisters(2, 132, 6);
    //  if (answer == node.ku8MBSuccess) {
    //    for (int Aj = 0; Aj < 6; Aj++) {
    //      DATA1[Aj] = node.getResponseBuffer(Aj);
    //    }
    //    uint32_t combined = DATA1[1];
    //    combined = (combined << 16) | DATA1[0];
    //    memcpy(&tmp8, &combined, 4);// VLL Average
    //    Serial.print("   VLL :  ");
    //    Serial.println(tmp8, 2);
    //
    //  }
    //  delay(1000);
    float VRU = preferences.getString("VRU").toFloat();
    float VYU = preferences.getString("VYU").toFloat();
    float VBU = preferences.getString("VBU").toFloat();
    float RYU = preferences.getString("RYU").toFloat();
    float YBU = preferences.getString("YBU").toFloat();
    float BRU = preferences.getString("BRU").toFloat();
    float VRL = preferences.getString("VRL").toFloat();
    float VYL = preferences.getString("VYL").toFloat();
    float VBL = preferences.getString("VBL").toFloat();
    float RYL = preferences.getString("RYL").toFloat();
    float YBL = preferences.getString("YBL").toFloat();
    float BRL = preferences.getString("BRL").toFloat();
    float CR = preferences.getString("CR").toFloat();
    float CY = preferences.getString("CY").toFloat();
    float CB = preferences.getString("CB").toFloat();

    Serial.println("Thresholds : VRU=" + String(VRU) + ",VYU=" + String(VYU) + ",VBU=" + String(VBU) + ",RYU=" + String(RYU) + ",YBU=" + String(YBU) + ",BRU=" + String(BRU) + ",VRL=" + String(VRL) + ",VYL=" + String(VYL) + ",VBL=" + String(VBL) + ",RYL=" + String(RYL) + ",YBL=" + String(YBL) + ",BRL=" + String(BRL) + ",CR=" + String(CR) + ",CY=" + String(CY) + ",CB=" + String(CB));

    float VR_ = 0.0;
    float VY_ = 0.0;
    float VB_ = 0.0;
    float VRY_ = 0.0;
    float VYB_ = 0.0;
    float VBR_ = 0.0;
    float CR_ = 0.0;
    float CY_ = 0.0;
    float CB_ = 0.0;

    server.handleClient();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 100, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("1 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&wattstotal, &combined, 4);  // WattsTotal
      // Serial.print("Wattstotal :  ");
      // Serial.print(wattstotal, 2);
      // Serial.println();
      strValues += String(wattstotal, 2) + String(",");
      docu["wattstotal"] = String(wattstotal, 2);
    } else {
      // Serial.println("1 : " + String(answer));
      // doc["Test"] = "Test-Hello";
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 116, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("2 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&disp_pf_avg, &combined, 4);  // Displacement PF avg
      // Serial.print("Displacement PF avg :  ");
      // Serial.print(disp_pf_avg, 2);
      // Serial.println();
      strValues += String(disp_pf_avg, 2) + String(",");
      docu["disp_pf_avg"] = String(disp_pf_avg, 2);
    } else {
      // Serial.println("2 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 124, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("3 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vatotal, &combined, 4);  // VA total
      // Serial.print("VA total :  ");
      // Serial.print(vatotal, 2);
      // Serial.println();
      strValues += String(vatotal, 2) + String(",");
      docu["vatotal"] = String(vatotal, 2);
    } else {
      // Serial.println("3 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 126, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("4 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&varphase, &combined, 4);  // VA R phase
      // Serial.print("VA R phase :  ");
      // Serial.print(varphase, 2);
      // Serial.println();
      strValues += String(varphase, 2) + String(",");
      docu["varphase"] = String(varphase, 2);
    } else {
      // Serial.println("4 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 128, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("5 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vayphase, &combined, 4);  // VA Y phase
      // Serial.print("VA Y phase :  ");
      // Serial.print(vayphase, 2);
      // Serial.println();
      strValues += String(vayphase, 2) + String(",");
      docu["vayphase"] = String(vayphase, 2);
    } else {
      // Serial.println("5 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 130, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("6 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vabphase, &combined, 4);  // VA B phase
      // Serial.print("VA B phase :  ");
      // Serial.print(vabphase, 2);
      // Serial.println();
      strValues += String(vabphase, 2) + String(",");
      docu["vabphase"] = String(vabphase, 2);
    } else {
      // Serial.println("6 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 132, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("7 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vllavg, &combined, 4);  // VLL AVG
      // Serial.print("VLL AVG :  ");
      // Serial.print(vllavg, 2);
      // Serial.println();
      strValues += String(vllavg, 2) + String(",");
      docu["vllavg"] = String(vllavg, 2);
    } else {
      // Serial.println("7 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 134, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("8 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vryphase, &combined, 4);  // Voltage of RY phase
      // Serial.print("VRY phase :  ");
      // Serial.print(vryphase, 2);
      // Serial.println();
      strValues += String(vryphase, 2) + String(",");
      docu["vryphase"] = String(vryphase, 2);
      VRY_ = String(vryphase, 2).toFloat();
    } else {
      // Serial.println("8 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 136, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("9 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vybphase, &combined, 4);  // Voltage of YB phase
      // Serial.print("VYB phase :  ");
      // Serial.print(vybphase, 2);
      // Serial.println();
      strValues += String(vybphase, 2) + String(",");
      docu["vybphase"] = String(vybphase, 2);
      VYB_ = String(vybphase, 2).toFloat();
    } else {
      // Serial.println("9 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 138, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("10 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vbrphase, &combined, 4);  // Voltage of BR phase
      // Serial.print("VBR phase :  ");
      // Serial.print(vbrphase, 2);
      // Serial.println();
      strValues += String(vbrphase, 2) + String(",");
      docu["vbrphase"] = String(vbrphase, 2);
      VBR_ = String(vbrphase, 2).toFloat();
    } else {
      // Serial.println("10 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 140, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("11 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vlnavg, &combined, 4);  // VLN AVG
      // Serial.print("VLN AVG :  ");
      // Serial.print(vlnavg, 2);
      // Serial.println();
      strValues += String(vlnavg, 2) + String(",");
      docu["vlnavg"] = String(vlnavg, 2);
    } else {
      // Serial.println("11 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 142, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("12 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vrphase, &combined, 4);  // Voltage of R Phase
      // Serial.print("VR Phase :  ");
      // Serial.print(vrphase, 2);
      // Serial.println();
      strValues += String(vrphase, 2) + String(",");
      docu["vrphase"] = String(vrphase, 2);
      VR_ = String(vrphase, 2).toFloat();
    } else {
      // Serial.println("12 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    fetchAndPublishSignalStrength();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 144, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("13 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vyphase, &combined, 4);  // Voltage of Y Phase
      // Serial.print("VY Phase :  ");
      // Serial.print(vyphase, 2);
      // Serial.println();
      strValues += String(vyphase, 2) + String(",");
      docu["vyphase"] = String(vyphase, 2);
      VY_ = String(vyphase, 2).toFloat();
    } else {
      // Serial.println("13 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 146, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("14 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&vbphase, &combined, 4);  // Voltage of B Phase
      // Serial.print("VB Phase :  ");
      // Serial.print(vbphase, 2);
      // Serial.println();
      strValues += String(vbphase, 2) + String(",");
      docu["vbphase"] = String(vbphase, 2);
      VB_ = String(vbphase, 2).toFloat();
    } else {
      // Serial.println("14 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 148, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("15 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&iavg, &combined, 4);  // Current Average
      // Serial.print("Current Average :  ");
      // Serial.print(iavg, 2);
      // Serial.println();
      strValues += String(iavg, 2) + String(",");
      docu["iavg"] = String(iavg, 2);
    } else {
      // Serial.println("15 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 150, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("16 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&irphase, &combined, 4);  // Current R phase
      // Serial.print("Current R phase :  ");
      // Serial.print(irphase, 2);
      // Serial.println();
      strValues += String(irphase, 2) + String(",");
      docu["irphase"] = String(irphase, 2);
      CR_ = String(irphase, 2).toFloat();
    } else {
      // Serial.println("16 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 152, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("17 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&iyphase, &combined, 4);  // Current Y phase
      // Serial.print("Current Y phase :  ");
      // Serial.print(iyphase, 2);
      // Serial.println();
      strValues += String(iyphase, 2) + String(",");
      docu["iyphase"] = String(iyphase, 2);
      CY_ = String(iyphase, 2).toFloat();
    } else {
      // Serial.println("17 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 154, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("18 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&ibphase, &combined, 4);  // Current B phase
      // Serial.print("Current B phase :  ");
      // Serial.print(ibphase, 2);
      // Serial.println();
      strValues += String(ibphase, 2) + String(",");
      docu["ibphase"] = String(ibphase, 2);
      CB_ = String(ibphase, 2).toFloat();
    } else {
      // Serial.println("18 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 156, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("19 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&freq, &combined, 4);  // Frequency
      // Serial.print("Frequency :  ");
      // Serial.print(freq, 2);
      // Serial.println();
      strValues += String(freq, 2) + String(",");
      docu["freq"] = String(freq, 2);
    } else {
      // Serial.println("19 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 158, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("20 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&iewh, &combined, 4);  // Import Energy WH
      // Serial.print("Import Energy WH :  ");
      // Serial.print(iewh, 2);
      // Serial.println();
      strValues += String(iewh, 2) + String(",");
      docu["iewh"] = String(iewh, 2);
    } else {
      // Serial.println("20 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 160, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("21 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&ievah, &combined, 4);  // Import Energy vah
      // Serial.print("Import Energy VAH :  ");
      // Serial.print(ievah, 2);
      // Serial.println();
      strValues += String(ievah, 2) + String(",");
      docu["ievah"] = String(ievah, 2);
    } else {
      // Serial.println("21 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 216, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("22 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&ilh, &combined, 4);  // Import Load Hour
      // Serial.print("Import Load Hour :  ");
      // Serial.print(ilh, 2);
      // Serial.println();
      strValues += String(ilh, 2) + String(",");
      docu["ilh"] = String(ilh, 2);
    } else {
      // Serial.println("22 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 350, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("23 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&co2, &combined, 4);  // CO2
      // Serial.print("CO2 :  ");
      // Serial.print(co2, 2);
      // Serial.println();
      strValues += String(co2, 2) + String(",");
      docu["co2"] = String(co2, 2);
    } else {
      // Serial.println("23 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();
    //  *****************************************************************
    answer = node.readHoldingRegisters(2, 352, 6);
    if (answer == node.ku8MBSuccess) {
      // Serial.println("24 : " + String(answer));
      for (int Aj = 0; Aj < 6; Aj++) {
        DATA1[Aj] = node.getResponseBuffer(Aj);
      }
      uint32_t combined = DATA1[1];
      combined = (combined << 16) | DATA1[0];
      memcpy(&tpfavg, &combined, 4);  // True PF Average
      // Serial.print("True PF Average :  ");
      // Serial.print(tpfavg, 2);
      // Serial.println();
      strValues += String(tpfavg, 2);
      docu["tpfavg"] = String(tpfavg, 2);
    } else {
      // Serial.println("24 : " + String(answer));
      Serial.println("Parameter Read Failure...!!!");
      return "";
    }
    delay(200);
    server.handleClient();
    loopMqtt();

    // docu["signal"] = fetchSignalStrength();
    // delay(200);
    // server.handleClient();
    // loopMqtt();

    // char jsonBufferr[1024];
    // serializeJson(docu, jsonBufferr);
    String jsonString;
    serializeJson(docu, jsonString);

    Serial.println("Readings : " + String(jsonString));

    // CR_ = 20.00;
    // CY_ = 20.00;
    // CB_ = 20.00;

    if (mqtt.connected()) {
      if (CR_ > 0 && CY_ > 0 && CB_ > 0) {
        /* Publish the data */
        Serial.println("Published Readings : " + String(jsonString));
        String readingsData = initiateEncryption(String(jsonString));
        mqtt.publish("Status", readingsData.c_str());
      } else {
        Serial.println("Meter Readings Not Published because Current is not flowing through RYB phases.");
      }
    } else {
      Serial.println("Meter Readings Not Published because MQTT disconnected.");
    }

    fetchAndPublishSignalStrength();

    if (CR_ > 0 && CY_ > 0 && CB_ > 0) {
      if (VR_ < VRL || VR_ > VRU) {
        turnOffMotor(String(VRL), String(VR_), String(VRU), "VR");
      } else if (VY_ < VYL || VY_ > VYU) {
        turnOffMotor(String(VYL), String(VY_), String(VYU), "VY");
      } else if (VB_ < VYL || VB_ > VBU) {
        turnOffMotor(String(VYL), String(VB_), String(VBU), "VB");
      } else if (VRY_ < RYL || VRY_ > RYU) {
        turnOffMotor(String(RYL), String(VRY_), String(RYU), "RY");
      } else if (VYB_ < YBL || VYB_ > YBU) {
        turnOffMotor(String(YBL), String(VYB_), String(YBU), "YB");
      } else if (VBR_ < BRL || VBR_ > BRU) {
        turnOffMotor(String(BRL), String(VBR_), String(BRU), "BR");
      } else if (CR_ > CR) {
        turnOffMotor2(String(CR_), String(CR), "R");
      } else if (CY_ > CY) {
        turnOffMotor2(String(CY_), String(CY), "Y");
      } else if (CB_ > CB) {
        turnOffMotor2(String(CB_), String(CB), "B");
      }
    } else {
      Serial.println("Thresholds Not Checked because Current is not flowing through RYB phases..");
    }
    Serial.println("*****************************************************************");
    return strValues;
  } else {
    server.handleClient();
    loopMqtt();
    Serial.println("current is not passing through the RYB phases i.e., currentAverage is not greater than 0.");
    return "";
  }
}

void turnOffMotor(String lowerThreshold, String readValue, String upperThreshold, String phase) {
  Serial.println("*********** Turning OFF Motor due to voltage mismatch***********");
  cumulativeWaterDischarge = 0.00;
  bPumpStatus = false;
  digitalWrite(RELAY_14, HIGH);
  DynamicJsonDocument doc(1024);
  doc["id"] = String(MAC_ID);
  doc["info"] = "Threshold has been breached";
  doc["voltage_phase"] = phase;
  doc["lowerThreshold"] = lowerThreshold;
  doc["upperThreshold"] = upperThreshold;
  doc["readingFromMeter"] = readValue;
  String jsonBuffer;
  serializeJson(doc, jsonBuffer);
  if (mqtt.connected()) {
    String alert = initiateEncryption(String(jsonBuffer));
    mqtt.publish("Status", alert.c_str());
  }
  delay(3000);
  digitalWrite(RELAY_14, LOW);
  delay(1000);
}

void turnOffMotor2(String readCurrent, String thresholdCurrent, String phase) {
  Serial.println("*********** Turning OFF Motor due to current mismatch***********");
  cumulativeWaterDischarge = 0.00;
  bPumpStatus = false;
  digitalWrite(RELAY_14, HIGH);
  DynamicJsonDocument doc(1024);
  doc["id"] = String(MAC_ID);
  doc["info"] = "Threshold has been breached";
  doc["current_phase"] = phase;
  doc["threshold"] = thresholdCurrent;
  doc["readingFromMeter"] = readCurrent;
  String jsonBuffer;
  serializeJson(doc, jsonBuffer);
  if (mqtt.connected()) {
    String alert = initiateEncryption(String(jsonBuffer));
    mqtt.publish("Status", alert.c_str());
  }
  delay(3000);
  digitalWrite(RELAY_14, LOW);
  delay(1000);
}

void fetchAndPublishSignalStrength() {

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= signalPublishInterval) {
    // Serial.println("Signal fetching interval has reached. " + String(currentMillis - previousMillis) + " ms");
    previousMillis = currentMillis;
    if (mqtt.connected()) {
      String signalQuality;
      long rssi = WiFi.RSSI();
      Serial.print("WiFi Signal Strength: " + String(rssi) + String(" dBm "));

      if (rssi >= -50 && rssi <= -1) {
        Serial.println("(Strong)");
        signalQuality = "Strong";
      } else if (rssi >= -70 && rssi <= -51) {
        Serial.println("(Good)");
        signalQuality = "Good";
      } else if (rssi <= -71) {
        Serial.println("(Weak)");
        signalQuality = "Weak";
      } else {
        Serial.println("(No Signal)");
        signalQuality = "No Signal";
      }

      // String signalStrength = String(rssi) + " dBm " + signalQuality;

      DynamicJsonDocument doc(1024);
      doc["id"] = String(MAC_ID);
      doc["info"] = "Signal info";
      doc["RSSI"] = String(rssi) + " dBm";
      doc["signalStrength"] = signalQuality;
      String jsonBuffer;
      serializeJson(doc, jsonBuffer);
      if (mqtt.connected()) {
        String signal = initiateEncryption(String(jsonBuffer));
        mqtt.publish("Status", signal.c_str());
        Serial.println("Published Signal Info : " + String(jsonBuffer));
      }
    } else {
      Serial.println("Signal strength reading was bypassed due to the unavailability of an MQTT connection.");
    }
  } else {
    // Serial.println("Signal fetching interval has not yet been reached.");
  }
}

double GetWaterDischarge() {
  server.handleClient();
  float nLitres = 0;
  if ((millis() - oldTime) > 999) {  // Update for every 2 seconds
    unsigned long ms = millis() - oldTime;
    detachInterrupt(digitalPinToInterrupt(flowMeterPin));
    calibrationFactor = preferences.getString("CF").toFloat();
    flowRate = ((1000.0 / ms) * pulseCount) / calibrationFactor;
    nLitres = flowRate * ms / 60000;
    cumulativeWaterDischarge += nLitres;

    Serial.print("Pulse Count : ");
    Serial.println(pulseCount);
    Serial.print("Calibration Factor : ");
    Serial.println(calibrationFactor, 5);
    Serial.print("Milli Seconds : ");
    Serial.println(ms);
    Serial.print("Flow rate: ");
    Serial.print(flowRate);
    Serial.println(" L/min");
    Serial.print("Number of Litres : ");
    Serial.println(nLitres);
    Serial.print("Number of litres (cumulative) : ");
    Serial.println(cumulativeWaterDischarge, 2);
    pulseCount = 0;
    oldTime = millis();
    attachInterrupt(digitalPinToInterrupt(flowMeterPin), pulseCounter, RISING);
  }
  // Serial.println("Method GetWaterDischarge Ended.");
  // return nLitres;
  return cumulativeWaterDischarge;
}

void loopMqtt() {
  if (mqtt.connected()) {
    // Serial.println("=== MQTT CONNECTED ===");
    mqtt.loop();
  } else {
    Serial.println("=== MQTT DISCONNECTED ===");
  }
}

// Check for update by comparing versions
bool checkForUpdate() {
  HTTPClient http;
  http.begin(version_url);
  // http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // Enable following redirects
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
    String newLocation = http.header("Location");
    Serial.println("Redirected to: " + newLocation);
    http.end();
    http.begin(newLocation);  // Follow the new URL
    httpCode = http.GET();
  }
  Serial.println("httpCode: " + String(httpCode));
  if (httpCode == HTTP_CODE_OK) {
    String latestVersion = http.getString();
    latestVersion.trim();  // Remove whitespace or newline characters
    Serial.println("Latest Version: " + latestVersion);
    Serial.println("Current Version: " + currentVersion);
    return (latestVersion != currentVersion);
  } else {
    Serial.println("Failed to check for updates. HTTP Code: " + String(httpCode));
    return false;
  }
}

// Perform OTA update
void performOTA() {
  HTTPClient http;
  http.begin(firmware_url);
  // http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // Enable following redirects
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
    String newLocation = http.header("Location");
    Serial.println("Redirected to: " + newLocation);
    http.end();
    http.begin(newLocation);  // Follow the new URL
    httpCode = http.GET();
  }
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      Serial.println("Starting OTA...");
      WiFiClient& client = http.getStream();
      size_t written = Update.writeStream(client);

      if (written == contentLength && Update.end()) {
        Serial.println("OTA Update Success. Restarting...");
        ESP.restart();
      } else {
        Serial.println("OTA Update Failed. Error #: " + String(Update.getError()));
      }
    } else {
      Serial.println("Not enough space for OTA update.");
    }
  } else {
    Serial.println("Failed to fetch firmware. HTTP Code: " + String(httpCode));
  }

  http.end();
}

/* -------------------------- Encryption - Decryption Methods  -----------------------*/

void hashKey(const char* key, byte* hashedKey, size_t keyLength) {
  SHA256 sha256;
  sha256.update((const uint8_t*)key, keyLength);  // Corrected method
  sha256.finalize(hashedKey, 32);                 // Generate a 32-byte key
}

// Function to pad the input string to a multiple of 16 bytes
void padString(String& input) {
  int paddingLength = 16 - (input.length() % 16);
  for (int i = 0; i < paddingLength; i++) {
    input += (char)paddingLength;
  }
}

// Function to remove padding from the decrypted string
void unpadString(String& input) {
  int paddingLength = input[input.length() - 1];
  if (paddingLength > 0 && paddingLength <= 16) {
    input.remove(input.length() - paddingLength);
  }
}

// Function to encrypt a string using AES-256-CBC
String encryptString(const String& plainText, const byte* key, const byte* iv) {
  AES aes;
  aes.set_key(key, 32);  // Set 256-bit key

  // Pad the plaintext
  String paddedText = plainText;
  padString(paddedText);

  int paddedLength = paddedText.length();
  byte encryptedBytes[paddedLength];

  // Create a mutable copy of IV since CBC mode modifies it
  byte ivCopy[16];
  memcpy(ivCopy, iv, 16);

  // Encrypt the padded text
  aes.cbc_encrypt((byte*)paddedText.c_str(), encryptedBytes, paddedLength / 16, ivCopy);

  // Convert encrypted bytes to hex string
  String encryptedText;
  for (int i = 0; i < paddedLength; i++) {
    char hex[3];
    sprintf(hex, "%02X", encryptedBytes[i]);
    encryptedText += hex;
  }

  return encryptedText;
}

// Function to decrypt a string using AES-256-CBC
String decryptString(const String& encryptedText, const byte* key, const byte* iv) {
  AES aes;
  aes.set_key(key, 32);  // Set 256-bit key

  // Convert hex string to byte array
  int encryptedLength = encryptedText.length() / 2;
  byte encryptedBytes[encryptedLength];

  for (int i = 0; i < encryptedLength; i++) {
    char hex[3] = { encryptedText[i * 2], encryptedText[i * 2 + 1], '\0' };
    encryptedBytes[i] = (byte)strtol(hex, NULL, 16);
  }

  // Decrypt the bytes
  byte decryptedBytes[encryptedLength];

  // Create a fresh IV copy for decryption
  byte ivCopy[16];
  memcpy(ivCopy, iv, 16);

  aes.cbc_decrypt(encryptedBytes, decryptedBytes, encryptedLength / 16, ivCopy);

  // Convert decrypted bytes to string
  String decryptedText;
  for (int i = 0; i < encryptedLength; i++) {
    decryptedText += (char)decryptedBytes[i];
  }

  // Remove padding
  unpadString(decryptedText);

  return decryptedText;
}

/* ----------------------------------------------------------------------------------*/