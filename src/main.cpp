// General project definitions
#include "common.hpp"

// Peripherie

// Include Other parts

// Globals
OPERATION_MODE op_mode;
AirQualityWifi air_wifi;
#ifdef OLED_AVAILABLE
Display display;
#endif
ConfigMode config_mode;
SerialTerminal terminal;
Config configuration;
MQTT mqtt;
TTN ttn;
HardwareSerial SerialSensor(2);

// Timing data
unsigned long last_read = 0;
unsigned long read_interval = 0;

// Measurement state
boolean measurement_running = false;

Sensor* sensor;
AirSensor* air_sensor;

boolean ledState=true;

boolean lora_en=false;
boolean mqtt_en=false;
boolean oled_en=false;

void setup() {
  Serial.begin(115200);
  // Setup Configuration
  configuration.init();
  #ifdef LORA_AVAILABLE
  lora_en = configuration.getBool("LORA_ENABLED");
  #endif
  #ifdef OLED_AVAILABLE
  oled_en = true;
  #endif
  Heltec.begin(oled_en,lora_en,false);
  delay(1);
  Serial.printf("Boot with LORA: %d\n",lora_en);
  Serial.printf("Boot with OLED: %d\n",oled_en);
  terminal.init();
  // Status LED
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,ledState);
  // sensor->init(); ToDo: Actively initialize the sensor if necessary
  mqtt_en = configuration.getBool("MQTT_ENABLED");
  // Check Mode
  if(configuration.getBool("CONFIGURED")) {
    // Start normal operation
    Serial.println("operation mode");
    op_mode = normal;
      // Choose particle sensor
    switch(configuration.getInt("SENSOR_TYPE")) {
      case 4:
        sensor = new SPS30();
        break;
      case 3:
        sensor = new HPMA115C0();
        break;
      case 2:
        sensor = new Sds_011();
        break;
      case 1:
        sensor = new DemoSensor();
        break;
      default:
        sensor = new DefaultSensor();
    }
    // Choose air sensor
    switch(configuration.getInt("AIR_SENSOR_TYPE")) {
      case 1:
        air_sensor = new BME680();
      break;
      default:
        air_sensor = new AirDefaultSensor();
    }
  } else {
    // Enter config mode
    Serial.println("configuration mode");
    op_mode = config;
  }
  air_wifi.init();
  #ifdef OLED_AVAILABLE
  display.init();
  #endif
  
  // Readout Timing
  read_interval = configuration.getInt("READ_INTERVAL")*1000;

  if(op_mode == config) {
    config_mode.init();
  } else {
    // Initialize MQTT if enabled
    if(mqtt_en) {
      mqtt.init();
    }
    // Initialize LoRa if enabled
    if(lora_en) {
      ttn.init();
    }
  }
  /* 

  // Print out success
  Heltec.display->clear();
  Heltec.display->drawString(0,0,"Connected!");
  Heltec.display->drawString(0,10,WiFi.localIP().toString());
  Heltec.display->display();
  Serial.println("Connected!");
  Serial.println(WiFi.localIP().toString());
  // ToDo: PubSubClient */
}

void loop() {
  air_wifi.handle();
  if(op_mode == config) {
    config_mode.handle();
  } else {
    if(mqtt_en) {
      mqtt.handle();
    }
    if(lora_en) {
      ttn.handle();
    }
    sensor->handle();
    air_sensor->handle();
    // If there is an measurement running and (data available or timeout exceeded), send out data, or if timeout exceeded
    if(measurement_running && ((sensor->measurementStatus() && air_sensor->measurementStatus()) || (millis() - last_read > read_interval))) {
      if(millis() - last_read > read_interval) {
        Serial.println("Measurement: Timeout exceeded, sending anyway!");
      }
      measurement_running = false;
      // Send measurement data
      sensorData data = sensor->getData();
      airSensorData air_data = air_sensor->getData();
      if(mqtt_en) {
        mqtt.send(data,air_data);
      }
      if(lora_en) {
        ttn.send(data,air_data);
      }
    }
    // Check if it's time for an new readout and no measurement is running
    if((millis() - last_read > read_interval) && !measurement_running) {
      ledState = !ledState;
      last_read = millis();
      // Start measurement
      sensor->startMeasurement();
      air_sensor->startMeasurement();
      // Check if we have an immediate result
      if(sensor->measurementStatus() && air_sensor->measurementStatus()) {
        // Send measurement data
        sensorData data = sensor->getData();
        airSensorData air_data = air_sensor->getData();
        if(mqtt_en) {
          mqtt.send(data,air_data);
        }
        if(lora_en) {
          ttn.send(data,air_data);
        }
      } else {
        // set flag
        measurement_running = true;
      }
    }
  }
  terminal.handle();
  digitalWrite(LED_BUILTIN,ledState);
}