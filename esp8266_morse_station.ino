#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pin Configuration (Maintained as requested)
#define BTN_DOT 13   // D7
#define BTN_DASH 12  // D6
#define BTN_MODE 0   // D3
#define LDR_PIN A0
#define SIGNAL_OUT 14 // D5

Adafruit_SSD1306 display(128, 64, &Wire, -1);
ESP8266WebServer server(80);

// Data Persistence (For Phone Dashboard)
String sentText = "";
String receivedText = "";

// Display Buffers (For OLED - Managed with Auto-Wrap)
String oledDisplay = ""; 
String currentMorse = "";

// State Management & Stability Timing
bool transmitMode = true; 
unsigned long lastPressTime = 0;
unsigned long pulseStartTime = 0;
unsigned long lastLDRRead = 0; // Throttle for WiFi stability
bool isInputActive = false;

const int letterTimeout = 1500;
const int dashThreshold = 300;
const int ldrSampleRate = 20; // 20ms gap to keep ADC/WiFi happy

bool lastDot = HIGH, lastDash = HIGH, lastMode = HIGH;

// --- WEB DASHBOARD HTML ---
String getHTML() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{font-family:sans-serif; text-align:center; background:#f0f2f5; padding:20px;}";
  html += ".card{background:white; margin-bottom:20px; padding:15px; border-radius:12px; box-shadow:0 4px 6px rgba(0,0,0,0.1); border-top: 5px solid #1a73e8;}";
  html += "h1{color:#1a73e8;} .history{font-size:22px; color:#333; min-height:40px; word-wrap: break-word; background:#fafafa; padding:10px; border-radius:8px;}</style>";
  html += "<script>setInterval(function(){location.reload();}, 3000);</script></head><body>";
  html += "<h1>Morse Station Dashboard</h1>";
  html += "<div class='card'><h3>System Status: <span style='color:" + String(transmitMode ? "#28a745" : "#dc3545") + ";'>" + String(transmitMode ? "TRANSMITTING" : "RECEIVING") + "</span></h3></div>";
  html += "<div class='card'><h3>SENT HISTORY:</h3><div class='history'>" + (sentText == "" ? "---" : sentText) + "</div></div>";
  html += "<div class='card'><h3>RECEIVED HISTORY:</h3><div class='history'>" + (receivedText == "" ? "---" : receivedText) + "</div></div>";
  html += "</body></html>";
  return html;
}

void setup() {
  pinMode(BTN_DOT, INPUT_PULLUP);
  pinMode(BTN_DASH, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(SIGNAL_OUT, OUTPUT);
  digitalWrite(SIGNAL_OUT, LOW);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(true);

  // Start Access Point
  WiFi.softAP("Morse_Master_Station", "12345678");
  server.on("/", [](){ server.send(200, "text/html", getHTML()); });
  server.begin();

  refreshOLED("STATION READY");
}

void loop() {
  server.handleClient(); // Essential for Web UI
  
  // 1. Mode Toggle Logic
  bool currentModeBtn = digitalRead(BTN_MODE);
  if (currentModeBtn == LOW && lastMode == HIGH) {
    transmitMode = !transmitMode;
    oledDisplay = ""; // Clear OLED buffer
    currentMorse = ""; // Clear Morse buffer
    refreshOLED(transmitMode ? "TX START" : "RX START");
    delay(250); // Debounce
  }
  lastMode = currentModeBtn;

  // 2. Logic Selection (With WiFi Stability Throttle)
  if (transmitMode) {
    handleTransmit();
  } else {
    // Only sample the LDR every 20ms to prevent WiFi stack crash
    if (millis() - lastLDRRead > ldrSampleRate) {
      handleReceive();
      lastLDRRead = millis();
    }
  }

  // 3. Decoding Timeout (Character Finalization)
  if (currentMorse.length() > 0 && (millis() - lastPressTime > letterTimeout)) {
    char letter = decodeMorse(currentMorse);
    
    // Update Persistent Logs
    if (transmitMode) sentText += letter;
    else receivedText += letter;
    
    // Update OLED with Auto-Wrap (20 chars per "page")
    oledDisplay += letter;
    if (oledDisplay.length() > 20) {
      oledDisplay = String(letter); 
    }
    
    refreshOLED(oledDisplay);
    currentMorse = "";
  }

  // 4. THE CRITICAL FIX: Hand control back to WiFi stack
  delay(1); 
  yield(); 
}

void handleTransmit() {
  if (digitalRead(BTN_DOT) == LOW && lastDot == HIGH) triggerSignal(".", 150);
  if (digitalRead(BTN_DASH) == LOW && lastDash == HIGH) triggerSignal("-", 400);
  lastDot = digitalRead(BTN_DOT); lastDash = digitalRead(BTN_DASH);
}

void handleReceive() {
  int ldrVal = analogRead(LDR_PIN);
  bool light = (ldrVal < 500); // Adjust threshold based on ambient light
  
  if (light && !isInputActive) {
    pulseStartTime = millis();
    isInputActive = true;
    digitalWrite(SIGNAL_OUT, HIGH); // Buzzer/Laser feedback
  }
  if (!light && isInputActive) {
    unsigned long dur = millis() - pulseStartTime;
    currentMorse += (dur < dashThreshold) ? "." : "-";
    lastPressTime = millis();
    isInputActive = false;
    digitalWrite(SIGNAL_OUT, LOW);
  }
}

void triggerSignal(String sym, int ms) {
  currentMorse += sym;
  lastPressTime = millis();
  digitalWrite(SIGNAL_OUT, HIGH);
  delay(ms);
  digitalWrite(SIGNAL_OUT, LOW);
}

void refreshOLED(String text) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println(transmitMode ? "[ TX MODE ]" : "[ RX MODE ]");
  display.println("---------------------");
  display.setTextSize(2);
  display.setCursor(0, 25);
  display.println(text);
  display.display();
}

char decodeMorse(String m) {
  if (m == ".-") return 'A'; if (m == "-...") return 'B'; if (m == "-.-.") return 'C';
  if (m == "-..") return 'D'; if (m == ".") return 'E'; if (m == "..-.") return 'F';
  if (m == "--.") return 'G'; if (m == "....") return 'H'; if (m == "..") return 'I';
  if (m == ".---") return 'J'; if (m == "-.-") return 'K'; if (m == ".-..") return 'L';
  if (m == "--") return 'M'; if (m == "-.") return 'N'; if (m == "---") return 'O';
  if (m == ".--.") return 'P'; if (m == "--.-") return 'Q'; if (m == ".-.") return 'R';
  if (m == "...") return 'S'; if (m == "-") return 'T'; if (m == "..-") return 'U';
  if (m == "...-") return 'V'; if (m == ".--") return 'W'; if (m == "-..-") return 'X';
  if (m == "-.--") return 'Y'; if (m == "--..") return 'Z';
  return ' ';
}
