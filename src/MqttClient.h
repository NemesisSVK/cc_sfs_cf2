#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "Logger.h"

class MqttClient
{
   private:
    WiFiClient          wifiClient;
    PubSubClient        client;
    bool                enabled;
    String              server;
    int                 port;
    String              username;
    String              password;
    String              clientId;
    String              topicPrefix;
    unsigned long       lastReconnectAttempt;
    const unsigned long RECONNECT_INTERVAL = 30000;  // 30 seconds

    void callback(char* topic, byte* payload, unsigned int length);
    bool reconnect();
    void publishSensorData();

   public:
    MqttClient();
    ~MqttClient();

    void begin();
    void loop();
    void updateSettings(bool enabled, const String& server, int port, const String& username,
                        const String& password, const String& clientId, const String& topicPrefix);

    // Data publishing methods
    void publishMovementData(bool movementDetected);
    void publishRunoutData(bool filamentRunout);
    void publishConnectionData(bool isConnected);
    void publishPrinterStatus(const String& status);
    void publishSystemHealth(int heapUsage, int wifiSignal);

    // Home Assistant MQTT discovery
    void publishHADiscovery();
    void publishHASensorConfig(const String& sensorName, const String& friendlyName,
                               const String& deviceClass, const String& unit = "",
                               const String& icon = "");

    bool   isConnected();
    String getStatus();
};

#endif