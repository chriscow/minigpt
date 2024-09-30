#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "Wire.h"

#include <vector>
#include <string>

#define ST(A) #A
#define STR(A) ST(A)


#include <WiFiManager.h>



// Define the SSID and password for the AP
const char* ap_ssid = "MiniGPT_Setup";
const char* ap_password = "12345678"; // Minimum 8 characters

WebServer server(80);
bool wifiConnected = false;


// Constants for OpenAI
const char *openai_api_key = STR(OPENAI_API_KEY); 
const char *openai_host = "api.openai.com";
const int openai_port = 443;
const char *openai_path = "/v1/chat/completions";

const int leftMargin = 5;  // Left margin for text display


// Constants for Wi-Fi
const char* ssid = STR(WIFI_SSID);
const char* password = STR(WIFI_PASSWORD);

// Setup for Keyboard I2C communication
#define I2C_DEV_ADDR 0x55

TTGOClass *ttgo = nullptr;
TFT_eSPI *tft;

String keyValue;
char key_ch = ' ';

// Setup for HTTPS client
WiFiClientSecure client;


const char *rootCACertificate = \
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

#include <vector>
#include <string>

struct Line {
    String text;
    uint16_t color;
};
std::vector<Line> lineBuffer;  // Buffer to store lines of text
String currentLine = "";  // This stores the incomplete line being processed

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
    tft->print(line.text);
}



void scrollUpDisplay() {
    // Shift all lines up by one
    for (size_t i = 0; i < lineBuffer.size(); i++) {
        printLineToTFT(i, lineBuffer[i]);
    }
    // Clear the last content line (before the input line)
    int yPos = (numLinesOnScreen - 1) * fontHeight;
    tft->fillRect(0, yPos, tft->width(), fontHeight, TFT_BLACK);
}



void addLineToBuffer(String line, uint16_t color) {
    Serial.println("Adding line to buffer: " + line);

    // If buffer is full, scroll up
    if (lineBuffer.size() >= numLinesOnScreen - 2) {  // Reserve last line for input
        Serial.println("Buffer full before adding new line.");
        // Remove the first line from the buffer
        lineBuffer.erase(lineBuffer.begin());
        // Scroll up the display
        scrollUpDisplay();
        Serial.println("Scrolled up display after removing first line.");
    }

    // Add the new line to the buffer
    lineBuffer.push_back({line, color});

    Serial.println("Current lineBuffer contents after adding new line:");
    for (size_t i = 0; i < lineBuffer.size(); i++) {
        Serial.printf("Buffer Line %d: %s\n", i, lineBuffer[i].text.c_str());
    }

    // Calculate the line index
    int lineIndex = lineBuffer.size() - 1;
    if (lineIndex < 0) {
        lineIndex = 0;
    }
    if (lineIndex >= numLinesOnScreen - 2) {
        lineIndex = numLinesOnScreen - 2;  // Adjust to prevent overwriting the input line
    }

    // Print the new line at the correct position
    printLineToTFT(lineIndex, lineBuffer[lineIndex]);
    Serial.printf("Printed line at index %d: %s\n", lineIndex, line.c_str());
}

void updateLastLine(Line line) {
    int lineIndex = lineBuffer.size() - 1;
    if (lineIndex < 0) {
        lineIndex = 0;
    }
    if (lineIndex >= numLinesOnScreen - 2) {
        lineIndex = numLinesOnScreen - 2;  // Adjust to prevent overwriting the input line
    }
    Serial.printf("Updating last line at index %d with currentLine: %s\n", lineIndex, line.text.c_str());
    printLineToTFT(lineIndex, line);
}



void processTextChunk(String chunk) {
    Serial.println("Entering processTextChunk with chunk: " + chunk);
    int maxWidth = 35;  // Adjust based on font and screen width

    // Append the incoming chunk to the current line
    currentLine += chunk;

    Serial.println("Updated currentLine: " + currentLine);

    // Process any newline characters in currentLine
    int newlinePos;
    while ((newlinePos = currentLine.indexOf('\n')) != -1) {
        String linePart = currentLine.substring(0, newlinePos);
        currentLine = currentLine.substring(newlinePos + 1);
        addLineToBuffer(linePart, TFT_LIGHTGREY);
    }

    // While currentLine is longer than maxWidth, extract full lines
    while (currentLine.length() >= maxWidth) {
        String linePart = currentLine.substring(0, maxWidth);
        currentLine = currentLine.substring(maxWidth);
        addLineToBuffer(linePart, TFT_LIGHTGREY);
    }

    // Update the last line on the screen with the remaining currentLine
    updateLastLine({currentLine, TFT_LIGHTGREY});
}



void sendQueryToOpenAI(String query) {
    Serial.println("Entering sendQueryToOpenAI");
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

    Serial.println("Request sent to OpenAI");

    // Wait for the OpenAI API response
    bool headersReceived = false;
    String line = "";

    // Read the response from the server
    while (client.connected()) {
        line = client.readStringUntil('\n');

        // Serial.println("Received line: " + line); // Log each line received

        // Skip HTTP headers
        if (!headersReceived) {
            if (line == "\r") {
                headersReceived = true;
                Serial.println("Headers received, streaming content...");
                // addLineToBuffer("MiniGPT:", TFT_GREEN);
                // addLineToBuffer("", TFT_GREEN);
                // printLineToTFT(lineBuffer.size() - 1, "OpenAI says:");
            } else if (line.startsWith("HTTP/1.1")) {
                Serial.println("HTTP Response: " + line);
            } else if (line.startsWith("Content-Type:")) {
                Serial.println("Content-Type: " + line);
            }
        } else {
            // Parse each streamed chunk of JSON
            if (line.startsWith("data: ")) {
                line = line.substring(6); // Remove the "data: " prefix

                // Serial.println("Data chunk: " + line); // Log the data chunk

                // End of stream is marked by 'data: [DONE]'
                if (line == "[DONE]") {
                    Serial.println("Stream finished.");
                    break;
                }

                // Parse the JSON chunk
                StaticJsonDocument<512> doc; // Reduced size to save memory
                DeserializationError error = deserializeJson(doc, line);
                if (error) {
                    Serial.print(F("deserializeJson() failed: "));
                    Serial.println(error.c_str());
                    Serial.println("Received invalid JSON: " + line);
                    continue;
                }

                // Extract the 'content' field from the JSON response
                JsonVariant delta = doc["choices"][0]["delta"];
                if (!delta.isNull()) {
                    const char* content = delta["content"];
                    if (content && strlen(content) > 0) {
                        // Process the content chunk and add it to the buffer when a complete line is formed
                        processTextChunk(String(content));
                        // Serial.print("Received content: ");
                        Serial.println(content);  // Print to serial monitor for debugging
                    } else {
                        Serial.println("Content is null or empty");
                    }
                } else {
                    Serial.println("Delta is null");
                }


            } else if (line.length() > 0) {
                Serial.println("Non-data line: " + line);
            }
        }

        // Check if the connection has been closed by the server
        if (!client.connected()) {
            Serial.println("Connection closed by server");
            break;
        }
    }



    client.stop();

    // At the end of the OpenAI response
    if (currentLine.length() > 0) {
        Serial.println("End of chat with last line remaining: " + currentLine);
        addLineToBuffer(currentLine, TFT_LIGHTGREY);
        currentLine = "";
    }

    currentLine = "";
    Serial.println("OpenAI response complete.");
}


void handleKeyPress() {
    Wire.requestFrom(I2C_DEV_ADDR, 1);
    while (Wire.available() > 0) {
        key_ch = Wire.read();
        if (key_ch == (char)0x00) {
            return;
        }

        // Print the character and its ASCII code for debugging
        Serial.print("Key pressed: ");
        Serial.print(key_ch);            // Print the character
        Serial.print(" (ASCII code: ");
        Serial.print((int)key_ch);       // Print the ASCII code
        Serial.println(")");

        // Handle special keys like backspace or enter
        if (key_ch == '\n' || key_ch == 0x0D || key_ch == 0x0A) { // Add possible Enter keys
            Serial.println("Enter key detected.");
            if (keyValue.length() > 0) {

                addLineToBuffer("", TFT_YELLOW);
                addLineToBuffer("> " + keyValue, TFT_YELLOW);

                sendQueryToOpenAI(keyValue);
                keyValue = ""; // Clear input after sending
                updateInputLine(keyValue);
            } else {
                Serial.println("Empty input, skipping.");
            }
        } else if (key_ch == 0x08) { // Handle backspace
            if (!keyValue.isEmpty()) {
                keyValue.remove(keyValue.length() - 1);
                updateInputLine(keyValue);
            }
        } else {
            keyValue += key_ch;
            Serial.printf("keyValue: %s\n", keyValue.c_str());
            updateInputLine(keyValue);
        }
    }
}


// bool connectAndRun(String ssid, String password) {
//     Serial.println("Connecting to Wi-Fi...");
//     WiFi.begin(ssid.c_str(), password.c_str());

//     int wifiAttempts = 0;
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//         if (++wifiAttempts > 20) {
//             Serial.println("Failed to connect to Wi-Fi.");
//             return false;
//         }
//     }

//     Serial.println("Connected to Wi-Fi!");
//     Serial.print("IP Address: ");
//     Serial.println(WiFi.localIP());

//     // Proceed with your main application logic

//     // Initialize secure client
//     client.setCACert(rootCACertificate); // Add your root certificate here if needed

//     // Send an initial query to OpenAI
//     addLineToBuffer("BlestX MiniGPT", TFT_CYAN);
//     sendQueryToOpenAI("Hello, please introduce yourself to me.");

//     return true;
// }



void setup() {
    Serial.begin(115200);
    delay(1000); // Delay for 1 seconds to allow time to connect a serial monitor

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
    Serial.print("Number of lines on screen: ");
    Serial.println(numLinesOnScreen);

    WiFiManager wm;

    // Automatically connect using saved credentials,
    // or start the configuration portal if none are saved
    if (!wm.autoConnect("MiniGPT")) {
        tft->fillScreen(TFT_BLACK);
        printLineToTFT(1, {"            MiniGPT Setup\n", TFT_CYAN});
        printLineToTFT(3, {"Failed to connect to Wi-Fi network", TFT_RED});

        Serial.println("Failed to connect or timeout occurred");
        return;
    }
    else
    {
        Serial.println("Connected to Wi-Fi");

        client.setCACert(rootCACertificate); // Add your root certificate here if needed

        // Send an initial query to OpenAI
        addLineToBuffer("            BlestX MiniGPT", TFT_CYAN);
        sendQueryToOpenAI("Hello, please introduce yourself to me.");
    }
}



void loop() {

    // Continuously read keyboard input
    handleKeyPress();
}
