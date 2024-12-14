#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <Adafruit_I2CDevice.h>
#include <SPI.h>
#include <HTTPClient.h>

// HardwareSerial and Adafruit_Fingerprint setup
HardwareSerial serialPort(2); // use UART2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&serialPort);

// WiFi credentials
const char* ssid = "POCO F4";
const char* password = "ashwin123";

// Flask server endpoint
const char* serverURL = "http://192.168.187.167:5000/upload-template";

// Global variable for fingerprint ID
uint8_t id;

// Function declarations
uint8_t getFingerprintEnroll();
uint8_t readnumber();
uint8_t downloadFingerprintTemplate(uint16_t id);

void sendToFlask(const String& id, uint8_t* templateData, size_t size);

void setup() {
    Serial.begin(9600);
    while (!Serial);
    delay(100);

    Serial.println("\n\nAdafruit Fingerprint sensor enrollment");

    // Initialize fingerprint sensor
    finger.begin(57600);
    if (finger.verifyPassword()) {
        Serial.println("Found fingerprint sensor!");
    } else {
        Serial.println("Did not find fingerprint sensor :(");
        while (1) {
            delay(1);
        }
    }

    // Connect to WiFi
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");

    Serial.println("Ready to enroll a fingerprint!");
}

void loop() {
    Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
    id = readnumber();
    if (id == 0) {
        return; // ID #0 not allowed
    }

    Serial.print("Enrolling ID #");
    Serial.println(id);

    while (!getFingerprintEnroll());
}
uint8_t getFingerprintEnroll() {
    int p = -1;
    Serial.print("Waiting for valid finger to enroll as #");
    Serial.println(id);

    while (p != FINGERPRINT_OK) {
        p = finger.getImage();
        switch (p) {
            case FINGERPRINT_OK:
                Serial.println("Image taken");
                break;
            case FINGERPRINT_NOFINGER:
                Serial.println(".");
                break;
            case FINGERPRINT_PACKETRECIEVEERR:
                Serial.println("Communication error");
                break;
            case FINGERPRINT_IMAGEFAIL:
                Serial.println("Imaging error");
                break;
            default:
                Serial.println("Unknown error");
                break;
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
    while (finger.getImage() != FINGERPRINT_NOFINGER);

    Serial.println("Place the same finger again");
    p = -1;
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
                break;
            case FINGERPRINT_IMAGEFAIL:
                Serial.println("Imaging error");
                break;
            default:
                Serial.println("Unknown error");
                break;
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

    Serial.print("Creating model for #");
    Serial.println(id);
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

    // Store the model in the sensor
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

    // Download and send the fingerprint template
    p = downloadFingerprintTemplate(id);
    if (p != FINGERPRINT_OK) {
        Serial.println("Failed to download fingerprint template");
        return p;
    }

    return true;
}
uint8_t downloadFingerprintTemplate(uint16_t id) {
    Serial.println("------------------------------------");
    Serial.print("Attempting to load #");
    Serial.println(id);
    uint8_t p = finger.loadModel(id);
    switch (p) {
        case FINGERPRINT_OK:
            Serial.print("Template ");
            Serial.print(id);
            Serial.println(" loaded");
            break;
        case FINGERPRINT_PACKETRECIEVEERR:
            Serial.println("Communication error");
            return p;
        default:
            Serial.print("Unknown error ");
            Serial.println(p);
            return p;
    }

    Serial.print("Attempting to get #");
    Serial.println(id);
    p = finger.getModel();
    switch (p) {
        case FINGERPRINT_OK:
            Serial.print("Template ");
            Serial.print(id);
            Serial.println(" transferring:");
            break;
        default:
            Serial.print("Unknown error ");
            Serial.println(p);
            return p;
    }

    // Buffer to store received data
    uint8_t bytesReceived[534]; // Adjust size based on your sensor
    memset(bytesReceived, 0xff, sizeof(bytesReceived));
    uint32_t startTime = millis();
    int i = 0;
    while (i < sizeof(bytesReceived) && (millis() - startTime) < 20000) {
        if (serialPort.available()) {
            bytesReceived[i++] = serialPort.read();
        }
    }
    Serial.print(i);
    Serial.println(" bytes read.");

    // Print the template in hexadecimal format
    Serial.println("Fingerprint Template Data (Hex):");
    for (int j = 0; j < sizeof(bytesReceived); j++) {
        Serial.print("0x");
        if (bytesReceived[j] < 0x10) {
            Serial.print("0"); // Add leading zero for single-digit hex values
        }
        Serial.print(bytesReceived[j], HEX);
        Serial.print(" ");
        if ((j + 1) % 16 == 0) {
            Serial.println(); // Newline after every 16 bytes
        }
    }
    Serial.println();

    // Send the template to the Flask server
    sendToFlask(String(id), bytesReceived, sizeof(bytesReceived));

    return p;
}

void sendToFlask(const String& id, uint8_t* templateData, size_t size) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverURL);
        http.addHeader("Content-Type", "application/json"); // Set content type to JSON

        // Convert template data to a string
        String fingerprintString = "";
        for (size_t i = 0; i < size; i++) {
            if (templateData[i] < 0x10) {
                fingerprintString += "0"; // Add leading zero for single-digit hex values
            }
            fingerprintString += String(templateData[i], HEX);
            if (i < size - 1) {
                fingerprintString += " "; // Add space between bytes
            }
        }

        // Prepare the JSON payload
        String payload = "{\"FingerprintID\": \"" + id + "\", \"FingerprintData\": \"" + fingerprintString + "\"}";

        // Send POST request
        int httpResponseCode = http.POST(payload);
        if (httpResponseCode > 0) {
            Serial.printf("Response from server: %d\n", httpResponseCode);
            String response = http.getString();
            Serial.println("Server response: " + response);
        } else {
            Serial.printf("Error sending data to server: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    } else {
        Serial.println("WiFi not connected");
    }
}


uint8_t readnumber() {
    uint8_t num = 0;
    while (num == 0) {
        while (!Serial.available());
        num = Serial.parseInt();
    }
    return num;
}
