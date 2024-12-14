#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_MLX90614.h>

// HardwareSerial and Adafruit_Fingerprint setup
HardwareSerial serialPort(2); // use UART2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&serialPort);

// MLX90614 Infrared Sensor setup
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// WiFi credentials
const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";

// Flask server endpoint
const char* serverURL = "http://<your-flask-server-ip>:5000/verify-template";

uint8_t getFingerprintID();
void fetchFingerprintDataFromServer();

void setup()
{
    Serial.begin(9600);
    while (!Serial)
        ; // For Yun/Leo/Micro/Zero/...
    delay(100);
    Serial.println("\n\nAdafruit finger detect test");

    // Initialize fingerprint sensor
    finger.begin(57600);
    delay(5);
    if (finger.verifyPassword())
    {
        Serial.println("Found fingerprint sensor!");
    }
    else
    {
        Serial.println("Did not find fingerprint sensor :(");
        while (1)
        {
            delay(1);
        }
    }

    // Connect to WiFi
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");

    fetchFingerprintDataFromServer();

    Serial.println(F("Reading sensor parameters"));
    finger.getParameters();
    Serial.print(F("Status: 0x"));
    Serial.println(finger.status_reg, HEX);
    Serial.print(F("Sys ID: 0x"));
    Serial.println(finger.system_id, HEX);
    Serial.print(F("Capacity: "));
    Serial.println(finger.capacity);
    Serial.print(F("Security level: "));
    Serial.println(finger.security_level);
    Serial.print(F("Device address: "));
    Serial.println(finger.device_addr, HEX);
    Serial.print(F("Packet len: "));
    Serial.println(finger.packet_len);
    Serial.print(F("Baud rate: "));
    Serial.println(finger.baud_rate);

    finger.getTemplateCount();

    if (finger.templateCount == 0)
    {
        Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
    }
    else
    {
        Serial.println("Waiting for valid finger...");
        Serial.print("Sensor contains ");
        Serial.print(finger.templateCount);
        Serial.println(" templates");
    }

    // Initialize the MLX90614 sensor
    if (!mlx.begin())
    {
        Serial.println("Error connecting to MLX90614 sensor. Check wiring!");
        while (1)
        {
            delay(1);
        }
    }
    else
    {
        Serial.println("MLX90614 sensor initialized!");
    }
}

void loop()
{
    uint8_t result = getFingerprintID();

    if (result == FINGERPRINT_OK)
    {
        // If fingerprint match is successful, read temperature
        Serial.println("Scanning temperature...");

        float ambientTemp = mlx.readAmbientTempC();
        float objectTemp = mlx.readObjectTempC();

        Serial.print("Ambient Temperature: ");
        Serial.print(ambientTemp);
        Serial.println(" °C");

        Serial.print("Object Temperature: ");
        Serial.print(objectTemp);
        Serial.println(" °C");

        // Check if the temperature is within range
        if (objectTemp < 37.5)
        {
            Serial.println("Attendance recorded successfully!");
        }
        else
        {
            Serial.println("Attendance rejected: High temperature detected!");
        }
    }

    delay(2000); // Delay between scans
}

uint8_t getFingerprintID()
{
    uint8_t p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
    case FINGERPRINT_NOFINGER:
        Serial.println("No finger detected");
        return p;
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

    // OK success!

    p = finger.image2Tz();
    switch (p)
    {
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

    // OK converted!
    p = finger.fingerSearch();
    if (p == FINGERPRINT_OK)
    {
        Serial.println("Found a print match!");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
        return p;
    }
    else if (p == FINGERPRINT_NOTFOUND)
    {
        Serial.println("Did not find a match");
        return p;
    }
    else
    {
        Serial.println("Unknown error");
        return p;
    }

    // Found a match!
    Serial.print("Found ID #");
    Serial.print(finger.fingerID);
    Serial.print(" with confidence of ");
    Serial.println(finger.confidence);

    return FINGERPRINT_OK;
}

void fetchFingerprintDataFromServer()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        http.begin(serverURL);

        int httpResponseCode = http.GET();
        if (httpResponseCode > 0)
        {
            Serial.printf("Response from server: %d\n", httpResponseCode);
            String payload = http.getString();
            Serial.println("Received data: " + payload);

            // Process the received fingerprint data here if needed
        }
        else
        {
            Serial.printf("Error receiving data from server: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    }
    else
    {
        Serial.println("WiFi not connected");
    }
}
