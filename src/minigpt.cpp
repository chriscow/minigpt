#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "Wire.h"
#include <WiFiManager.h>
#include <vector>
#include <string>

#define ST(A) #A
#define STR(A) ST(A)

WebServer server(80);
bool wifiConnected = false;

// Constants for OpenAI
const char* openai_api_key = STR(OPENAI_API_KEY);
const char* openai_host = "api.openai.com";
const int openai_port = 443;
const char* openai_path = "/v1/chat/completions";

const int leftMargin = 5;  // Left margin for text display

// Constants for Wi-Fi
const char* ssid = STR(WIFI_SSID);
const char* password = STR(WIFI_PASSWORD);

// Setup for Keyboard I2C communication
#define I2C_DEV_ADDR 0x55

TTGOClass* ttgo = nullptr;
TFT_eSPI* tft;

String keyValue;
char key_ch = ' ';

// Setup for HTTPS client
WiFiClientSecure client;

const char* rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX\n" \
"MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE\n" \
"CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx\n" \
"NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT\n" \
"GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0\n" \
"MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube\n" \
"Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e\n" \
"WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H/MIH8MA4GA1UdDwEB/wQEAwIBhjAd\n" \
"BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd\n" \
"BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN\n" \
"l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhpodHRw\n" \
"Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v\n" \
"Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG\n" \
"SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ\n" \
"odI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1hY\n" \
"+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/Cs\n" \
"kGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbep\n" \
"8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K1\n" \
"vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl\n" \
"-----END CERTIFICATE-----\n";

struct Line {
    String text;
    uint16_t color;
};
std::vector<Line> lineBuffer;  // Buffer to store lines of text
String currentLine = "";       // This stores the incomplete line being processed

int numLinesOnScreen;  // Number of lines that fit on the screen (excluding input line)
int fontHeight;        // Height of the current font

void updateInputLine(String input) {
    int yPos = tft->height() - fontHeight;  // Position at the last line
    tft->fillRect(0, yPos, tft->width(), fontHeight, TFT_BLACK);
    tft->setCursor(leftMargin, yPos);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->print("> " + input);
}

void printLineToTFT(int lineIndex, Line line) {
    if (lineIndex >= numLinesOnScreen - 1) {
        lineIndex = numLinesOnScreen - 2;  // Prevent overwriting the input line
    }
    int yPos = lineIndex * fontHeight;
    tft->fillRect(0, yPos, tft->width(), fontHeight, TFT_BLACK);
    tft->setCursor(leftMargin, yPos);
    tft->setTextColor(line.color, TFT_BLACK);  // Use the color from the line
    tft->print(line.text.c_str());
}

void scrollUpDisplay() {
    for (size_t i = 0; i < lineBuffer.size(); i++) {
        printLineToTFT(i, lineBuffer[i]);
    }
    // Clear the last content line (before the input line)
    int yPos = (numLinesOnScreen - 1) * fontHeight;
    tft->fillRect(0, yPos, tft->width(), fontHeight, TFT_BLACK);
}

void addLineToBuffer(String line, uint16_t color) {
    if (lineBuffer.size() >= numLinesOnScreen - 1) {  // Reserve last line for input
        lineBuffer.erase(lineBuffer.begin());
        scrollUpDisplay();
    }
    lineBuffer.push_back({line, color});
    int lineIndex = lineBuffer.size() - 1;
    if (lineIndex < 0) {
        lineIndex = 0;
    }
    if (lineIndex >= numLinesOnScreen - 1) {
        lineIndex = numLinesOnScreen - 1;  // Adjust to prevent overwriting the input line
    }
    printLineToTFT(lineIndex, lineBuffer[lineIndex]);
}

void updateLastLine(Line line) {
    int lineIndex = lineBuffer.size();
    if (lineIndex < 0) {
        lineIndex = 0;
    }
    if (lineIndex >= numLinesOnScreen - 1) {
        lineIndex = numLinesOnScreen - 1;  // Adjust to prevent overwriting the input line
    }
    printLineToTFT(lineIndex, line);
}

void processTextChunk(String chunk) {
    int maxWidth = 35;  // Adjust based on font and screen width
    currentLine += chunk;

    // Process any newline characters in currentLine
    int newlinePos;
    while ((newlinePos = currentLine.indexOf('\n')) != -1) {
        String linePart = currentLine.substring(0, newlinePos);
        currentLine = currentLine.substring(newlinePos + 1);
        addLineToBuffer(linePart, TFT_LIGHTGREY);
    }

    // While currentLine is longer than maxWidth, extract full lines
    while (currentLine.length() >= maxWidth) {
        // Find the last space within maxWidth characters
        int spacePos = currentLine.lastIndexOf(' ', maxWidth - 1);
        int breakPos;
        if (spacePos != -1) {
            breakPos = spacePos;
        } else {
            // No space found, force break at maxWidth
            breakPos = maxWidth;
        }

        String linePart = currentLine.substring(0, breakPos);
        currentLine = currentLine.substring(breakPos);

        // Remove leading spaces from currentLine
        while (currentLine.length() > 0 && currentLine[0] == ' ') {
            currentLine = currentLine.substring(1);
        }

        addLineToBuffer(linePart, TFT_LIGHTGREY);
    }

    // Update the last line on the screen with the remaining currentLine
    updateLastLine({currentLine, TFT_LIGHTGREY});
}


void sendQueryToOpenAI(String query) {
    updateInputLine("");
    if (!client.connect(openai_host, openai_port)) {
        Serial.println("Connection to OpenAI API failed!");
        addLineToBuffer("Failed to connect to OpenAI API", TFT_RED);
        return;
    }
    // Prepare the JSON payload with the "stream" option set to true
    String payload = "{\"model\":\"gpt-3.5-turbo\",\"messages\":[{\"role\":\"system\",\"content\":\"You are MiniGPT, a chat interface running on an Espressif ESP32, inside of a LilyGo T-Watch.\"},{\"role\":\"user\",\"content\":\"" + query + "\"}],\"stream\":true}";
    // Send HTTP headers
    client.println("POST " + String(openai_path) + " HTTP/1.1");
    client.println("Host: " + String(openai_host));
    client.println("Authorization: Bearer " + String(openai_api_key));
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(payload.length()));
    client.println();
    client.println(payload);
    // Wait for the OpenAI API response
    bool headersReceived = false;
    String line = "";
    // Read the response from the server
    while (client.connected()) {
        line = client.readStringUntil('\n');
        // Skip HTTP headers
        if (!headersReceived) {
            if (line == "\r") {
                headersReceived = true;
                // addLineToBuffer("MiniGPT:", TFT_GREEN);
                // addLineToBuffer("", TFT_GREEN);
            }
        } else {
            // Parse each streamed chunk of JSON
            if (line.startsWith("data: ")) {
                line = line.substring(6); // Remove the "data: " prefix
                // End of stream is marked by 'data: [DONE]'
                if (line == "[DONE]") {
                    break;
                }
                // Parse the JSON chunk
                StaticJsonDocument<512> doc; // Reduced size to save memory
                DeserializationError error = deserializeJson(doc, line);
                if (error) {
                    continue;
                }
                // Extract the 'content' field from the JSON response
                JsonVariant delta = doc["choices"][0]["delta"];
                if (!delta.isNull()) {
                    const char* content = delta["content"];
                    if (content && strlen(content) > 0) {
                        // Process the content chunk and add it to the buffer when a complete line is formed
                        processTextChunk(String(content));
                    }
                }
            }
        }
        // Check if the connection has been closed by the server
        if (!client.connected()) {
            break;
        }
    }
    client.stop();
    // At the end of the OpenAI response
    if (currentLine.length() > 0) {
        addLineToBuffer(currentLine, TFT_LIGHTGREY);
        currentLine = "";
    }
    currentLine = "";
}

void handleKeyPress() {
    Wire.requestFrom(I2C_DEV_ADDR, 1);
    while (Wire.available() > 0) {
        key_ch = Wire.read();
        if (key_ch == (char)0x00) {
            return;
        }
        // Handle special keys like backspace or enter
        if (key_ch == '\n' || key_ch == 0x0D || key_ch == 0x0A) { // Add possible Enter keys
            if (keyValue.length() > 0) {
                addLineToBuffer("", TFT_YELLOW);
                addLineToBuffer("> " + keyValue, TFT_YELLOW);
                sendQueryToOpenAI(keyValue);
                keyValue = ""; // Clear input after sending
                updateInputLine(keyValue);
            }
        } else if (key_ch == 0x08) { // Handle backspace
            if (!keyValue.isEmpty()) {
                keyValue.remove(keyValue.length() - 1);
                updateInputLine(keyValue);
            }
        } else {
            keyValue += key_ch;
            updateInputLine(keyValue);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000); // Delay for 1 second to allow time to connect a serial monitor
    // Initialize TTGO watch and display
    ttgo = TTGOClass::getWatch();
    ttgo->begin();
    tft = ttgo->tft;
    tft->fillScreen(TFT_BLACK);
    ttgo->openBL();
    tft->setTextFont(2);
    tft->setRotation(1);
    fontHeight = tft->fontHeight();
    if (fontHeight == 0) {
        fontHeight = 16; // Default font height
    }
    numLinesOnScreen = tft->height() / fontHeight; // Calculate number of lines that fit on screen
    numLinesOnScreen -= 1; // Reserve the last line for input
    WiFiManager wm;
    // Automatically connect using saved credentials,
    // or start the configuration portal if none are saved
    if (!wm.autoConnect("MiniGPT")) {
        lineBuffer.clear();
        tft->fillScreen(TFT_BLACK);
        addLineToBuffer("            MiniGPT Setup\n", TFT_CYAN);
        addLineToBuffer("", TFT_CYAN);
        addLineToBuffer("Failed to connect to Wi-Fi network", TFT_RED);
        addLineToBuffer("", TFT_CYAN);
        addLineToBuffer("Connect to MiniGPT Wi-Fi to configure", TFT_YELLOW);
        Serial.println("Failed to connect or timeout occurred");
        return;
    } else {
        Serial.println("Connected to Wi-Fi");
        client.setCACert(rootCACertificate); // Add your root certificate here if needed
        // Send an initial query to OpenAI
        lineBuffer.clear();
        tft->fillScreen(TFT_BLACK);
        addLineToBuffer("            BlestX MiniGPT", TFT_CYAN);
        sendQueryToOpenAI("Hello, please introduce yourself to me.");
    }
}

void loop() {
    // Continuously read keyboard input
    handleKeyPress();
}
