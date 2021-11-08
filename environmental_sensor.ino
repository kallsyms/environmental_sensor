#include <WiFi.h>
#include <WebServer.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_VEML7700.h>

#include "secrets.h"

Adafruit_SCD30 scd30;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
InfluxDBClient client(influxdb_url, influxdb_org, influxdb_bucket, influxdb_token, InfluxDbCloud2CACert);

Point sensor("environment");

WebServer server(80);

unsigned scd30_temp_offset = 250; // seems to be high by ~2.5degC

void setup() {
  while (!Serial) { delay(10); }
  Serial.begin(115200);

  // Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.print("\nGot IP ");
  Serial.println(WiFi.localIP());

  sensor.addTag("device", "ESP32");

  // SCD30 (temp, RH, CO2)
  if (!scd30.begin()) {
    Serial.println("SCD30 not found. Halting.");
    while (1) { delay(10); }
  }
  scd30.setMeasurementInterval(4);
  scd30.setTemperatureOffset(scd30_temp_offset);
  Serial.println("SCD30 initialized");

  // VEML7700 (lux)
  if (!veml.begin()) {
    Serial.println("VEML7700 not found. Halting.");
    while (1) { delay(10); }
  }

  veml.setGain(VEML7700_GAIN_1_4);
  veml.setIntegrationTime(VEML7700_IT_100MS);

  Serial.println("VEML7700 initialized");

  server.on("/scd30_recal", handle_scd30_recal);
  server.on("/scd30_temp_offset", handle_scd30_temp_offset);
  server.onNotFound(handle_404);
  
  server.begin();
  Serial.println("HTTP server started");
}

unsigned tick = 0;

void loop() {
  tick++;
  
  server.handleClient();

  if (tick % 5 == 0) {
    sensor.clearFields();
  
    if (scd30.dataReady()){
      if (scd30.read()) {
        sensor.addField("temperature", scd30.temperature);
        sensor.addField("rh", scd30.relative_humidity);
        sensor.addField("co2", scd30.CO2);
      } else {
        Serial.println("Error reading from SCD30");
      }
    }
  
    sensor.addField("lux", veml.readLux());
  
    // TODO: check and reconnect wifi if needed
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
  }

  delay(1000);
}

void handle_scd30_recal() {
  scd30.forceRecalibrationWithReference(400);
  server.send(200, "text/plain", "ok");
}

void handle_404() {
  server.send(404, "text/plain", "Not found");
}
