#include <WiFi.h>

// ESP32 WiFi UART bridge for NUEDC car telemetry/tuning.
// PC connects to TCP port 3333; ESP32 bridges TCP <-> UART2.

#ifndef NUEDC_AP_SSID
#define NUEDC_AP_SSID "NUEDC-CAR-UART"
#endif

#ifndef NUEDC_AP_PASS
#define NUEDC_AP_PASS "change-me-1234"
#endif

static const char *AP_SSID = NUEDC_AP_SSID;
static const char *AP_PASS = NUEDC_AP_PASS;
static const uint16_t TCP_PORT = 3333;

static const int UART_RX_PIN = 16;  // ESP32 RX2, connect to MSPM0 UART TX
static const int UART_TX_PIN = 17;  // ESP32 TX2, connect to MSPM0 UART RX
static const uint32_t UART_BAUD = 115200;

WiFiServer server(TCP_PORT);
WiFiClient client;

void setup()
{
    Serial.begin(115200);
    Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.softAP(AP_SSID, AP_PASS);
    server.begin();
    server.setNoDelay(true);

    Serial.println();
    Serial.println("NUEDC ESP32 UART bridge ready");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("PASS: ");
    Serial.println(AP_PASS);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("TCP: ");
    Serial.println(TCP_PORT);
}

void loop()
{
    if (!client || !client.connected()) {
        if (client) client.stop();
        client = server.available();
        if (client) {
            client.setNoDelay(true);
            Serial.println("TCP client connected");
        }
    }

    while (client && client.connected() && client.available()) {
        Serial2.write((uint8_t)client.read());
    }

    uint8_t buf[128];
    while (Serial2.available()) {
        size_t n = Serial2.readBytes(buf, min((int)sizeof(buf), Serial2.available()));
        if (client && client.connected()) client.write(buf, n);
        Serial.write(buf, n); // local USB debug mirror, optional
    }
}
