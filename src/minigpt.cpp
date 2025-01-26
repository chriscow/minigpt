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

#define TFT_RETRO_GREEN      0x07E0      /*  51, 255,  51 */
#define TFT_RETRO_AMBER      0xFD60      /* 255, 176,   0 */
#define TFT_PET_PHOSPHOR    0xC6FF      /* 200, 220, 255 */

const int leftMargin = 10;  // Left margin for text display
const int rightMargin = 2;


bool goingRetro = false;
uint16_t textColor = TFT_LIGHTGREY;

WebServer server(80);
bool wifiConnected = false;

// Constants for OpenAI
const char* openai_api_key = STR(OPENAI_API_KEY);
const char* openai_host = "api.openai.com";
const int openai_port = 443;
const char* openai_path = "/v1/chat/completions";



// Setup for Keyboard I2C communication
#define I2C_DEV_ADDR 0x55

TTGOClass* ttgo = nullptr;
TFT_eSPI* tft;
WiFiManager wm;

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
int textFont = 2;      // Current font to use
int fontHeight;        // Height of the current font
int font2CharsPerLine = 40; // Number of characters per line for font size 2
int font4CharsPerLine = 20; // Number of characters per line for font size 4


// Scrolling control variables
int displayStartIndex = 0;  // Index of the first line to display
bool touchActive = false;
int prevTouchY = -1;
int currTouchY = -1;
int touchThreshold = 10; // Minimum movement in pixels to register a scroll


// Struct to hold conversation messages
struct ChatMessage {
    String role;
    String content;
};
std::vector<ChatMessage> conversationHistory;  // Conversation history

unsigned long lastBatteryCheck = 0;
const unsigned long BATTERY_CHECK_INTERVAL = 30000; 

void printLineToTFT(int lineIndexOnScreen, Line line) {
    if (lineIndexOnScreen >= numLinesOnScreen - 1) {
        lineIndexOnScreen = numLinesOnScreen - 2;  // Prevent overwriting the input line
    }
    int yPos = lineIndexOnScreen * fontHeight;

    // clear the left margin of any "dirt" from previous text
    tft->fillRect(0, yPos, leftMargin, fontHeight, TFT_BLACK);
    tft->setCursor(leftMargin, yPos);
    tft->setTextColor(line.color, TFT_BLACK);  // Use the color from the line
    tft->print(line.text.c_str());
    int16_t width = tft->textWidth(line.text.c_str());
    if (width + leftMargin < tft->width()) {
        // Clear the rest of the line
        tft->fillRect(width + leftMargin, yPos, tft->width() - width - leftMargin, fontHeight, TFT_BLACK);
    }
}

void updateInputLine(String input) {
    int yPos = tft->height() - fontHeight;  // Position at the last line

    // Clear the input line area
    tft->fillRect(0, yPos, tft->width(), fontHeight, TFT_BLACK);

    // Set text color
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);

    // Calculate maximum width for input text
    int maxWidthPixels = tft->width() - leftMargin - rightMargin;
    int promptWidth = tft->textWidth("> ");
    int availableWidth = maxWidthPixels - promptWidth;

    // Prepare the input text
    String inputText = input;
    bool trimmed = false;

    // Combine prompt and input text
    String displayText = "> " + inputText;

    // If the total text width exceeds the maximum width, start trimming
    int textWidth = tft->textWidth(displayText);
    if (textWidth > maxWidthPixels) {
        trimmed = true;

        // Width of the ellipsis
        int ellipsisWidth = tft->textWidth("...");

        // Available width for input text after accounting for prompt and ellipsis
        int inputAvailableWidth = maxWidthPixels - promptWidth - ellipsisWidth;

        // Trim the input text from the beginning until it fits
        int startIndex = 0;
        while (startIndex < inputText.length() && tft->textWidth(inputText.substring(startIndex)) > inputAvailableWidth) {
            startIndex++;
        }

        // Add ellipsis to the trimmed input text
        inputText = "..." + inputText.substring(startIndex);

        // Reassemble the display text
        displayText = "> " + inputText;

        // Ensure the display text now fits within the maximum width
        while (tft->textWidth(displayText) > maxWidthPixels && inputText.length() > 4) {
            // Remove one more character after the ellipsis
            inputText = "..." + inputText.substring(4); // Remove '...' and one more character
            displayText = "> " + inputText;
        }
    }

    // Set cursor to the left margin and print the display text
    tft->setCursor(leftMargin, yPos);
    tft->print(displayText);
}

void redrawDisplay() {
    // Clear the display area except the input line
    // tft->fillRect(0, 0, tft->width(), tft->height() - fontHeight, TFT_BLACK);
    int maxLines = min(numLinesOnScreen - 1, (int)lineBuffer.size() - displayStartIndex);
    for (int i = 0; i < maxLines; i++) {
        int lineIndex = displayStartIndex + i;
        if (lineIndex < lineBuffer.size()) {
            printLineToTFT(i, lineBuffer[lineIndex]);
        }
    }

    int16_t yPos = (numLinesOnScreen - 1) * fontHeight;
    tft->fillRect(0, yPos, tft->width(), fontHeight, TFT_BLACK);

    // Update the input line
    updateInputLine(keyValue);
}

void scrollUp() {
    if (displayStartIndex + numLinesOnScreen - 1 < lineBuffer.size()) {
        displayStartIndex++;
        redrawDisplay();
    }
}

void scrollDown() {
    if (displayStartIndex > 0) {
        displayStartIndex--;
        redrawDisplay();
    }
}


void handleTouchInput() {
    int16_t x, y;
    ttgo->getTouch(x, y);
    if (x == -1 && y == -1) {
        // No touch detected
        touchActive = false;
        prevTouchY = -1;
    } else {
        // Touch detected
        if (!touchActive) {
            // Touch just started
            touchActive = true;
            prevTouchY = y;
        } else {
            // Touch is ongoing
            currTouchY = y;
            int deltaY = currTouchY - prevTouchY;
            if (abs(deltaY) >= touchThreshold) {
                // User has dragged up or down
                if (deltaY > 0) {
                    // User dragged down
                    scrollDown();
                } else {
                    // User dragged up
                    scrollUp();
                }
                prevTouchY = currTouchY;
            }
        }
    }
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
    lineBuffer.push_back({line, color});
    const int MAX_BUFFER_LINES = 200; // Adjust as needed
    if (lineBuffer.size() > MAX_BUFFER_LINES) {
        // Remove oldest lines to prevent overflow
        lineBuffer.erase(lineBuffer.begin(), lineBuffer.begin() + (lineBuffer.size() - MAX_BUFFER_LINES));
    }
    // Keep the display at the bottom when new lines are added
    if (displayStartIndex + numLinesOnScreen - 1 >= lineBuffer.size() - 1) {
        displayStartIndex = max(0, (int)lineBuffer.size() - (numLinesOnScreen - 1));
        redrawDisplay();
    }
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

void processTextChunk(String chunk, String& assistantReply) {
    assistantReply += chunk;
    int maxWidthPixels = tft->width() - leftMargin - rightMargin; // Calculate usable width
    currentLine += chunk;

    // Process any newline characters in currentLine
    int newlinePos;
    while ((newlinePos = currentLine.indexOf('\n')) != -1) {
        String linePart = currentLine.substring(0, newlinePos);
        currentLine = currentLine.substring(newlinePos + 1);
        addLineToBuffer(linePart, textColor);
    }

    // Process text based on pixel width
    while (tft->textWidth(currentLine) > maxWidthPixels) {
        int breakPos = -1;
        int lastSpacePos = -1;
        int accumulatedWidth = 0;

        // Find the breaking point within the current line
        for (int i = 0; i < currentLine.length(); i++) {
            accumulatedWidth += tft->textWidth(String(currentLine[i]));

            // Track the last space position
            if (currentLine[i] == ' ') {
                lastSpacePos = i;
            }

            // If the accumulated width exceeds the max width
            if (accumulatedWidth > maxWidthPixels) {
                // Break at the last space if possible, otherwise force a break here
                breakPos = (lastSpacePos != -1) ? lastSpacePos : i;
                break;
            }
        }

        // Break the line at the determined position
        String linePart = currentLine.substring(0, breakPos);
        currentLine = currentLine.substring(breakPos);

        // Remove leading spaces from the remaining text
        while (currentLine.length() > 0 && currentLine[0] == ' ') {
            currentLine = currentLine.substring(1);
        }

        // Add the broken line to the buffer
        addLineToBuffer(linePart, textColor);
    }

    // Update the last line on the screen with the remaining currentLine
    updateLastLine({currentLine, textColor});
}



void pruneConversationHistory() {
    const int MAX_HISTORY_MESSAGES = 10; // Number of messages excluding the system prompt
    int messagesToRemove = conversationHistory.size() - (MAX_HISTORY_MESSAGES + 1);
    if (messagesToRemove > 0) {
        conversationHistory.erase(conversationHistory.begin() + 1, conversationHistory.begin() + 1 + messagesToRemove);
    }
}

void sendQueryToOpenAI(String query) {
    keyValue = "";
    if (!client.connect(openai_host, openai_port))
    {
        Serial.println("Connection to OpenAI API failed!");
        addLineToBuffer("Failed to connect to OpenAI API", TFT_RED);
        return;
    }

    // Build the JSON payload with the conversation history
    const size_t capacity = 8192; // Adjust as necessary based on expected conversation size
    DynamicJsonDocument payloadDoc(capacity);

    payloadDoc["model"] = STR(OPENAI_MODEL);
    payloadDoc["stream"] = true;
    JsonArray messages = payloadDoc.createNestedArray("messages");

    for (const auto& msg : conversationHistory) {
        JsonObject messageObj = messages.createNestedObject();
        messageObj["role"] = msg.role;
        messageObj["content"] = msg.content;
    }

    String payload;
    serializeJson(payloadDoc, payload);

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
    String assistantReply = "";  // Collect the assistant's reply

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
                        processTextChunk(String(content), assistantReply);
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
        addLineToBuffer(currentLine, textColor);
        currentLine = "";
    }
    currentLine = "";

    // Add the assistant's reply to the conversation history
    conversationHistory.push_back({"assistant", assistantReply});
    pruneConversationHistory();  // Prune history if necessary
}


void setTextFont(int fontNum) {
    textFont = fontNum;
    tft->setTextFont(textFont); // 1: 80 2: 40 4: 20  6: 8 chars per line
    fontHeight = tft->fontHeight();
    if (fontHeight == 0) {
        fontHeight = 16; // Default font height
    }    

    numLinesOnScreen = tft->height() / fontHeight; // Calculate number of lines that fit on screen
    numLinesOnScreen -= 1; // Reserve the last line for input

    lineBuffer.clear();
    tft->fillScreen(TFT_BLACK);
}

void addCenteredLineToBuffer(String line, uint16_t color) {
    int screenWidth = tft->width();
    int pixelsNeeded = (screenWidth - tft->textWidth(line)) / 2;
    int spaceWidth = tft->textWidth(" ");

    int spaces = pixelsNeeded / spaceWidth;

    String centeredLine = "";
    // Add spaces to center the text
    for (int i = 0; i < spaces; i++) {
        centeredLine += " ";
    }

    addLineToBuffer(centeredLine + line, color);
}

void handleRebootReset(String input) {
    if (input.equalsIgnoreCase("reboot")) {
        Serial.println("Rebooting...");
        delay(2000);
        ESP.restart(); // Or ESP.reboot() in some frameworks
    } else if (input.equalsIgnoreCase("reset") || input.equalsIgnoreCase("wifi")) {
        Serial.println("Resetting wifi and rebooting...");
        delay(2000);
        wm.resetSettings();
        ESP.restart();
    }
}


void handleBatteryInfo() {
    // jump back to the bottom
    int bottomIndex = max(0, (int)lineBuffer.size() - (numLinesOnScreen - 1));
    if (displayStartIndex != bottomIndex) {
        displayStartIndex = bottomIndex;
    // redrawDisplay();
    }

    float batteryVoltage = ttgo->power->getBattVoltage() / 1000.0;  // Convert to volts
    int batteryPercentage = ttgo->power->getBattPercentage();
    bool isCharging = ttgo->power->isChargeing();  // Note: this is the correct spelling in the library

    if (batteryPercentage > 100) {
        batteryPercentage = 100;
    }

    unsigned long currentMillis = millis();
    unsigned long seconds = currentMillis / 1000;

    conversationHistory.push_back({"developer", 
        "You have been running for " + String(seconds) + " seconds. " +
        "Your battery is at " + String(batteryPercentage) + "% and " +
        (isCharging ? "is charging." : "is not charging.") +
        " The battery voltage is " + String(batteryVoltage) + " volts." +
        " It's a tiny battery, so don't expect much."
    });

    conversationHistory.push_back({"developer", 
        "Commands the user can enter:"
        
        "- `go retro` to change the screen colors to a retro theme"
        "- `reboot` to reboot the device"
        "- `reset` or `wifi` to reset the Wi-Fi settings and reboot"
        "- `+` to toggle between font sizes"
    });
}

void handleGoRetro() {
    if (keyValue.equalsIgnoreCase("go retro")) {
        randomSeed(millis());
        uint16_t newColor;
        do {
            newColor = random(3) == 0 ? TFT_RETRO_GREEN : (random(2) == 0 ? TFT_RETRO_AMBER : TFT_PET_PHOSPHOR);
        } while (goingRetro && newColor == textColor);
        goingRetro = true;
        textColor = newColor;
    
        String colorString =  
            TFT_RETRO_GREEN ? 
            "monochrome P1 Phosphor (Green). Known as 'P1 phosphor green' " \
            "or commonly called 'monochrome green phosphor'. This " \
            "was used in many early terminals and computers like the " \
            "IBM 5151 monitor and was technically a yellow-green color." : 
            (textColor == TFT_RETRO_AMBER ? 
            "monochrome P3 Phosphor (Amber). Not actually yellow or gold, but a specific " \
            "orange-yellow color known as 'P3 phosphor' or 'amber monochrome'. " \
            "This was popular on monitors like the Princeton MAX-12 " \
            "and some Zenith monitors. The color was specifically " \
            "chosen because it was believed to cause less eye strain " \
            "than green phosphor." : 
            "monochrome Commodore PET P4 Phosphor. This was a unique color " \
            "used in the Commodore PET series of computers. P4 phosphor " \
            "was standard for black-and-white TVs and some early computer " \
            "monitors, producing a bluish-white glow.");

        conversationHistory.push_back({"developer", 
        "Your screen colors have been updated. Your background is black." \
        "Your text foreground color is " + colorString + " You " \
        "have a vintage flair about you. Tell the user a little about the " \
        "history of this color based on the above information."});
    }
}

void handleSerialInput() {
    while (Serial.available() > 0) {
        char serialChar = Serial.read();

        // Echo the character back to the serial monitor
        Serial.print(serialChar);

        if (serialChar == '\n' || serialChar == '\r') {
            // Handle Enter key
            if (!keyValue.isEmpty()) {
                handleBatteryInfo();
                handleGoRetro();

                addLineToBuffer("> " + keyValue, TFT_YELLOW);
                conversationHistory.push_back({"user", keyValue});
                pruneConversationHistory();

                String input = keyValue;
                updateInputLine("");
                sendQueryToOpenAI(input); // this will clear `keyValue`, that's why we save it in `input`
                handleRebootReset(input);
            }
        } else if (serialChar == '\b') {
            // Handle backspace
            if (!keyValue.isEmpty()) {
                keyValue.remove(keyValue.length() - 1);
                updateInputLine(keyValue);

                // Echo backspace to clear the character on the monitor
                Serial.print("\b \b");
            }
        } else {
            // Append regular characters to the input
            keyValue += serialChar;
            updateInputLine(keyValue);
        }
    }
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
                conversationHistory.push_back({"user", keyValue});
                pruneConversationHistory();  // Prune history if necessary
                updateInputLine(keyValue);

                // jump back to the bottom
                int bottomIndex = max(0, (int)lineBuffer.size() - (numLinesOnScreen - 1));
                if (displayStartIndex != bottomIndex) {
                    displayStartIndex = bottomIndex;
                    // redrawDisplay();
                }

                handleBatteryInfo();
                handleGoRetro();

                String input = keyValue;

                // Send the query to OpenAI after updating the display
                updateInputLine("");
                sendQueryToOpenAI(input);  // this will clear `keyValue`, that's why we save it in `input`
                handleRebootReset(input);
            }
        } else if (key_ch == 0x2B) { // Handle '+' key to make toggle the font size to 4 and back to 2
            if (textFont == 2) {
                setTextFont(4);
                sendQueryToOpenAI("I just switched to a smaller font so I can see 40 characters across the screen.");
            } else {
                setTextFont(2);
                sendQueryToOpenAI("I just switched to a bigger font so I can see what you are saying, you little thing, you.");
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

void configModeCallback (WiFiManager *wm) {
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());

    Serial.println(wm->getConfigPortalSSID());

    setTextFont(4);
    addCenteredLineToBuffer(String(STR(COMPANY_NAME)) + " " + String(STR(NAME)), TFT_CYAN);
    addCenteredLineToBuffer("Connect to", TFT_DARKGREY);
    addLineToBuffer("", TFT_CYAN);
    addCenteredLineToBuffer(STR(NAME), TFT_GREEN);
    addLineToBuffer("", TFT_CYAN);
    addCenteredLineToBuffer("Wi-Fi access point", TFT_DARKGREY);
    addCenteredLineToBuffer("to configure", TFT_DARKGREY);

    Serial.println("Failed to connect or timeout occurred");
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
    tft->setRotation(1);

    wm.setDebugOutput(true);
    wm.setAPCallback(configModeCallback);
    // wm.setConfigPortalTimeout(30);

    // Automatically connect using saved credentials,
    // or start the configuration portal if none are saved
    if (!wm.autoConnect(STR(NAME))) {
        Serial.println("autoConnect returned false and timed out");
        Serial.println("resetting and restarting");
        wm.resetSettings();
        ESP.restart();
    } else {
        Serial.println("Connected to Wi-Fi");
        client.setCACert(rootCACertificate); // Add your root certificate here if needed

        // Initialize conversation history with the system prompt
        conversationHistory.push_back({"developer",
            "You are " + String(STR(NAME)) + ", a chat interface running on an Espressif ESP32, "
            "inside of a very tiny computer with a tiny keyboard, about the size a "
            "mouse would use. Your screen is very tiny, and displays 40 "
            "characters wide and 20 rows. You are a humourous little guy and make off color "
            "jokes about your screen size. Size isn't everything, right?"
            "Your LLM model is " + String(STR(OPENAI_MODEL)) + ", of course. " 
            "I am a regular sized human.  You are the one that is small."
            });

        // Send an initial query to OpenAI
        setTextFont(2);

        String header = String(STR(COMPANY_NAME)) + " " + String(STR(NAME));
        if (goingRetro) {
            addCenteredLineToBuffer(header, textColor);
        } else {
            addCenteredLineToBuffer(header, TFT_CYAN);
        }
        sendQueryToOpenAI("Hello!  Please tell me about yourself and the hardware you are running on.");
    }
}

void testMargins() {
    tft->fillScreen(TFT_BLACK); // Clear the screen

    int maxWidth = tft->width(); // Get screen width
    int maxHeight = tft->height(); // Get screen height
    int yPos = 0; // Start at the top of the screen
    String sampleText = "Margin Test Line";

    // Iterate over left and right margin values
    for (int margin = 0; margin <= 10; margin++) { // Adjust `10` as needed for range
        tft->fillScreen(TFT_BLACK); // Clear the screen for each margin

        tft->drawRect(margin, 0, maxWidth - margin * 2, maxHeight, TFT_RED); // Draw a border

        // print text in the center with the current margin
        yPos = (maxHeight - tft->fontHeight()) / 2;
        tft->setCursor(margin, yPos);
        tft->setTextColor(TFT_GREEN, TFT_BLACK); // Green text for visibility
        sampleText = "Left: " + String(margin) + " | Right: " + String(margin);
        tft->print(sampleText);

        // // Print text with the current margins
        // for (int line = 0; line < maxHeight / tft->fontHeight(); line++) {
        //     yPos = line * tft->fontHeight();
        //     tft->fillRect(0, yPos, maxWidth, tft->fontHeight(), TFT_BLACK); // Clear line area

        //     tft->setCursor(margin, yPos);
        //     tft->setTextColor(TFT_GREEN, TFT_BLACK); // Green text for visibility
        //     tft->print(sampleText);

        //     int textWidth = tft->textWidth(sampleText);
        //     tft->setCursor(maxWidth - textWidth - margin, yPos);

        //     // set the sampleText to include the margin sizes
        //     sampleText = "Left: " + String(margin) + " | Right: " + String(margin);
        //     tft->print(sampleText);
        // }

        delay(3000); // Pause for 3 seconds before testing the next margin
    }

    tft->fillScreen(TFT_BLACK); // Clear the screen after testing
}


void loop() {
    // testMargins();

    handleKeyPress();
    handleTouchInput();
    handleSerialInput();  // Add this line for serial console input

    // Add battery monitoring
    unsigned long currentMillis = millis();
    if (currentMillis - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
        lastBatteryCheck = currentMillis;

        // Get battery readings
        float batteryVoltage = ttgo->power->getBattVoltage() / 1000.0;  // Convert to volts
        int batteryPercentage = ttgo->power->getBattPercentage();
        bool isCharging = ttgo->power->isChargeing();  // Note: this is the correct spelling in the library

        // Print battery status
        Serial.printf("Battery: %.2fV | %d%% | %s\n",
                      batteryVoltage,
                      batteryPercentage,
                      isCharging ? "Charging" : "Not Charging");
    }
}
