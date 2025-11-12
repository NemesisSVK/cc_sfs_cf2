#include "MqttClient.h"

#include <ArduinoJson.h>
#include <WiFi.h>

MqttClient::MqttClient() : client(wifiClient)
{
    enabled              = false;
    port                 = 1883;
    lastReconnectAttempt = 0;
}

MqttClient::~MqttClient()
{
    if (client.connected())
    {
        client.disconnect();
    }
}

void MqttClient::begin()
{
    client.setServer(server.c_str(), port);
    client.setCallback([this](char* topic, byte* payload, unsigned int length)
                       { this->callback(topic, payload, length); });
}

void MqttClient::loop()
{
    if (!enabled || server.length() == 0)
    {
        return;
    }

    if (!client.connected())
    {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= RECONNECT_INTERVAL)
        {
            lastReconnectAttempt = now;
            if (reconnect())
            {
                logger.log("MQTT connected successfully");
            }
            else
            {
                logger.log("MQTT connection failed, will retry in 30 seconds");
            }
        }
    }
    else
    {
        client.loop();
    }
}

void MqttClient::updateSettings(bool mqttEnabled, const String& mqttServer, int mqttPort,
                                const String& mqttUsername, const String& mqttPassword,
                                const String& mqttClientId, const String& mqttTopicPrefix)
{
    bool settingsChanged = (enabled != mqttEnabled) || (server != mqttServer) ||
                           (port != mqttPort) || (username != mqttUsername) ||
                           (password != mqttPassword) || (clientId != mqttClientId) ||
                           (topicPrefix != mqttTopicPrefix);

    enabled     = mqttEnabled;
    server      = mqttServer;
    port        = mqttPort;
    username    = mqttUsername;
    password    = mqttPassword;
    clientId    = mqttClientId;
    topicPrefix = mqttTopicPrefix;

    if (settingsChanged)
    {
        if (client.connected())
        {
            client.disconnect();
        }
        if (enabled && server.length() > 0)
        {
            client.setServer(server.c_str(), port);
            logger.log("MQTT settings updated, reconnecting...");
        }
    }
}

bool MqttClient::reconnect()
{
    if (!enabled || server.length() == 0 || !WiFi.isConnected())
    {
        return false;
    }

    logger.logf("Attempting MQTT connection to %s:%d", server.c_str(), port);

    bool connected = false;
    if (username.length() > 0 && password.length() > 0)
    {
        connected = client.connect(clientId.c_str(), username.c_str(), password.c_str());
    }
    else
    {
        connected = client.connect(clientId.c_str());
    }

    if (connected)
    {
        // Publish online status
        String onlineTopic = topicPrefix + "/status";
        client.publish(onlineTopic.c_str(), "online", true);  // retained message

        // Publish availability status for HA
        String availabilityTopic = topicPrefix + "/" + clientId + "_death";
        client.publish(availabilityTopic.c_str(), "online", true);  // retained message

        // Subscribe to command topics if needed
        String commandTopic = topicPrefix + "/command";
        client.subscribe(commandTopic.c_str());

        // Publish Home Assistant discovery configs
        logger.log("Publishing Home Assistant discovery configs...");
        publishHADiscovery();

        logger.logf("MQTT connected as %s", clientId.c_str());
    }
    else
    {
        logger.logf("MQTT connection failed, state: %d", client.state());
    }

    return connected;
}

void MqttClient::callback(char* topic, byte* payload, unsigned int length)
{
    String topicStr = String(topic);
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char) payload[i];
    }

    logger.logf("MQTT message received: %s = %s", topic, message.c_str());

    // Handle commands if needed
    if (topicStr.endsWith("/command"))
    {
        // Add command handling logic here if needed
    }
}

void MqttClient::publishMovementData(bool movementDetected)
{
    if (!client.connected())
        return;

    String                 topic = topicPrefix + "/sensor/" + clientId + "_movement/state";
    StaticJsonDocument<32> doc;
    doc["value"] = movementDetected ? 1 : 0;
    String payload;
    serializeJson(doc, payload);
    client.publish(topic.c_str(), payload.c_str(), true);
}

void MqttClient::publishRunoutData(bool filamentRunout)
{
    if (!client.connected())
        return;

    String                 topic = topicPrefix + "/sensor/" + clientId + "_runout/state";
    StaticJsonDocument<32> doc;
    doc["value"] = filamentRunout ? 1 : 0;
    String payload;
    serializeJson(doc, payload);
    client.publish(topic.c_str(), payload.c_str(), true);
}

void MqttClient::publishConnectionData(bool isConnected)
{
    if (!client.connected())
        return;

    String                 topic = topicPrefix + "/sensor/" + clientId + "_connection/state";
    StaticJsonDocument<32> doc;
    doc["value"] = isConnected ? 1 : 0;
    String payload;
    serializeJson(doc, payload);
    client.publish(topic.c_str(), payload.c_str(), true);
}

void MqttClient::publishPrinterStatus(const String& status)
{
    if (!client.connected())
        return;

    String topic = topicPrefix + "/printer/status";
    client.publish(topic.c_str(), status.c_str(), true);
}

void MqttClient::publishSystemHealth(int heapUsage, int wifiSignal)
{
    if (!client.connected())
        return;

    // Publish heap usage
    String                 heapTopic = topicPrefix + "/sensor/" + clientId + "_heap_usage/state";
    StaticJsonDocument<32> heapDoc;
    heapDoc["value"] = heapUsage;
    String heapPayload;
    serializeJson(heapDoc, heapPayload);
    client.publish(heapTopic.c_str(), heapPayload.c_str(), true);

    // Publish WiFi signal
    String                 wifiTopic = topicPrefix + "/sensor/" + clientId + "_wifi_signal/state";
    StaticJsonDocument<32> wifiDoc;
    wifiDoc["value"] = wifiSignal;
    String wifiPayload;
    serializeJson(wifiDoc, wifiPayload);
    client.publish(wifiTopic.c_str(), wifiPayload.c_str(), true);
}

bool MqttClient::isConnected()
{
    return client.connected();
}

String MqttClient::getStatus()
{
    if (!enabled)
    {
        return "Disabled";
    }
    if (!WiFi.isConnected())
    {
        return "WiFi not connected";
    }
    if (client.connected())
    {
        return "Connected to " + server + ":" + String(port);
    }
    return "Disconnected from " + server + ":" + String(port);
}

void MqttClient::publishHADiscovery()
{
    if (!client.connected())
        return;

    logger.log("Starting Home Assistant discovery config publishing...");

    // Publish sensor configs one at a time with longer delays
    if (client.connected())
    {
        publishHASensorConfig("movement", "Movement Sensor", "", "", "mdi:motion-sensor");
        client.loop();
        delay(500);
    }

    if (client.connected())
    {
        publishHASensorConfig("runout", "Filament Runout", "", "", "mdi:printer-3d-nozzle-alert");
        client.loop();
        delay(500);
    }

    if (client.connected())
    {
        publishHASensorConfig("connection", "Printer Connection", "", "", "mdi:printer");
        client.loop();
        delay(500);
    }

    if (client.connected())
    {
        publishHASensorConfig("heap_usage", "Heap Usage", "", "%", "mdi:memory");
        client.loop();
        delay(500);
    }

    if (client.connected())
    {
        publishHASensorConfig("wifi_signal", "WiFi Signal", "signal_strength", "dBm", "mdi:wifi");
        client.loop();
    }

    logger.log("Completed Home Assistant discovery config publishing");
}

void MqttClient::publishHASensorConfig(const String& sensorName, const String& friendlyName,
                                       const String& deviceClass, const String& unit,
                                       const String& icon)
{
    if (!client.connected())
        return;

    // Use proper HA MQTT discovery format: homeassistant/sensor/{node_id}/{object_id}/config
    String configTopic = topicPrefix + "/sensor/" + clientId + "_" + sensorName + "/config";
    String stateTopic  = topicPrefix + "/sensor/" + clientId + "_" + sensorName + "/state";

    StaticJsonDocument<512> doc;

    // Match the EXACT field order from user's working example
    if (deviceClass.length() > 0)
    {
        doc["device_class"] = deviceClass;
    }
    doc["name"]        = friendlyName;
    doc["state_topic"] = stateTopic;
    if (unit.length() > 0)
    {
        doc["unit_of_measurement"] = unit;
    }
    doc["value_template"]     = "{{ value_json.value}}";
    doc["unique_id"]          = clientId + "_" + sensorName;
    doc["state_class"]        = "measurement";
    doc["availability_topic"] = topicPrefix + "/" + clientId + "_death";
    doc["default_entity_id"]  = "sensor." + clientId + "_" + sensorName;
    doc["entity_category"]    = "diagnostic";

    // Device information - match user's format exactly
    JsonObject device      = doc.createNestedObject("device");
    device["name"]         = clientId;
    device["model"]        = "CC SFS";
    device["manufacturer"] = "Elegoo";
    JsonArray identifiers  = device.createNestedArray("identifiers");
    identifiers.add(clientId);

    if (icon.length() > 0)
    {
        doc["icon"] = icon;
    }

    String payload;
    serializeJson(doc, payload);

    logger.logf("Publishing HA config for %s to %s", sensorName.c_str(), configTopic.c_str());
    logger.logf("Config payload length: %d", payload.length());
    // Log first 200 chars of payload for debugging
    String debugPayload = payload.substring(0, 200);
    logger.logf("Config payload start: %s", debugPayload.c_str());
    bool publishResult = client.publish(configTopic.c_str(), payload.c_str(), true);  // retained
    if (publishResult)
    {
        logger.logf("Publish result: SUCCESS");
    }
    else
    {
        logger.logf("Publish result: FAILED (state: %d)", client.state());
    }
}