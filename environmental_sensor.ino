#include <WiFi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_VEML7700.h>

#include "secrets.h"

Adafruit_SCD30 scd30;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
InfluxDBClient client(influxdb_url, influxdb_org, influxdb_bucket, influxdb_token, InfluxDbCloud2CACert);

Point sensor("environment");

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
  Serial.println("SCD30 initialized");

  // VEML7700 (lux)
  if (!veml.begin()) {
    Serial.println("VEML7700 not found. Halting.");
    while (1) { delay(10); }
  }

  veml.setGain(VEML7700_GAIN_1_4);
  veml.setIntegrationTime(VEML7700_IT_50MS);

  Serial.println("VEML7700 initialized");
}

void loop() {
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

  delay(5000);
}
