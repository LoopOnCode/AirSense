#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <bsec2.h>

#define IIC_SDA_PIN 5
#define IIC_SCL_PIN 6

#define LED_PIN 2
#define NUM_LEDS 13
#define RGB_BRIGHTNESS 255

#define BUTTON_PIN 9
#define BUZZER_PIN 10

Bsec2 envSensor;
String IAQ_ACCURACY_STATES[4] = {"Stabilizing", "Uncertain", "Calibrating", "Calibrated"};

Adafruit_NeoPixel ledStrip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

String iaq_accuracy(int input)
{
  return IAQ_ACCURACY_STATES[input];
}

String iaq_classification(int input)
{
  switch (input)
  {
  case 0 ... 50:
    return "Excellent";
  case 51 ... 100:
    return "Good";
  case 101 ... 150:
    return "Lightly polluted";
  case 151 ... 200:
    return "Moderately polluted";
  case 201 ... 250:
    return "Heavily polluted";
  case 251 ... 350:
    return " Severely polluted";
  case 351 ... 500:
    return "Extremely polluted";
  }
}

void update_led(int input)
{
  switch (input)
  {
  case 0 ... 50:
  case 51 ... 100:
    ledStrip.setPixelColor(0, ledStrip.Color(0, 255, 0)); // Green
    break;
  case 101 ... 150:
    ledStrip.setPixelColor(0, ledStrip.Color(255, 255, 0)); // Yellow
    break;
  case 151 ... 200:
    ledStrip.setPixelColor(0, ledStrip.Color(255, 122, 0)); // Amber
    break;
  case 201 ... 250:
    ledStrip.setPixelColor(0, ledStrip.Color(255, 0, 0)); // Red
    break;
  case 251 ... 350:
    ledStrip.setPixelColor(0, ledStrip.Color(153, 0, 76)); // Purple
    break;
  case 351 ... 500:
    ledStrip.setPixelColor(0, ledStrip.Color(175, 53, 0)); // Brown
    break;
  }
  ledStrip.show();
}

// Call back for when there is new data from BSEC sensor
void newBsecDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec)
{
  if (!outputs.nOutputs)
  {
    return;
  }

  Serial.println("Sensor outputs:");
  for (uint8_t i = 0; i < outputs.nOutputs; i++)
  {
    const bsecData output = outputs.output[i];
    switch (output.sensor_id)
    {
    case BSEC_OUTPUT_IAQ:
      // Index for Air Quality (0-500)
      Serial.println("\tIAQ: " + String(output.signal) + " - " + iaq_classification(output.signal));
      update_led(output.signal);
      // Accuracy status (0-3)
      Serial.println("\tAccuracy: " + String((int)output.accuracy) + " - " + iaq_accuracy(output.accuracy));
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
      Serial.println("\tTemperature: " + String(output.signal) + "°C");
      break;
    case BSEC_OUTPUT_RAW_PRESSURE:
      Serial.println("\tPressure: " + String(output.signal / 100.0) + "hPa");
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
      Serial.println("\tHumidity: " + String(output.signal) + "%");
      break;
    case BSEC_OUTPUT_RAW_GAS:
      Serial.println("\tGas Resistance: " + String(output.signal / 1000.0) + "KΩ");
      break;
    case BSEC_OUTPUT_STABILIZATION_STATUS:
      Serial.println("\tStabilization Status: " + String(output.signal));
      break;
    case BSEC_OUTPUT_CO2_EQUIVALENT:
      Serial.println("\tCO2 equiv: " + String(output.signal) + "ppm");
      break;
    case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
      Serial.println("\tBreath VOC equiv: " + String(output.signal) + "ppm");
      break;
    default:
      break;
    }
  }
}

void bsecError(void)
{
  Serial.println("BME680: Error! Halted.");
  while (true)
  {
    ledStrip.setPixelColor(0, ledStrip.Color(255, 0, 0));
    ledStrip.show();
    delay(500);
    ledStrip.setPixelColor(0, ledStrip.Color(0, 0, 0));
    ledStrip.show();
    delay(500);
  }
}

void checkBsecStatus(Bsec2 bsec)
{
  if (bsec.status < BSEC_OK)
  {
    Serial.println("BSEC Error Code: " + String(bsec.status));
    bsecError(); // Halt in case of failure
  }
  else if (bsec.status > BSEC_OK)
  {
    Serial.println("BSEC Warning Code: " + String(bsec.status));
  }

  if (bsec.sensor.status < BME68X_OK)
  {
    Serial.println("BME68X Error Code: " + String(bsec.sensor.status));
    bsecError(); // Halt in case of failure
  }
  else if (bsec.sensor.status > BME68X_OK)
  {
    Serial.println("BME68X Warning Code: " + String(bsec.sensor.status));
  }
}

void setupBsec()
{
  Wire.begin(IIC_SDA_PIN, IIC_SCL_PIN);

  bsecSensor sensorList[] = {
      BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
      BSEC_OUTPUT_RAW_GAS,
      BSEC_OUTPUT_STABILIZATION_STATUS,
      BSEC_OUTPUT_CO2_EQUIVALENT,
      BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY};

  if (!envSensor.begin(BME68X_I2C_ADDR_HIGH, Wire))
  {
    checkBsecStatus(envSensor);
  }

  // Subscribe to the desired BSEC2 outputs
  if (!envSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_LP))
  {
    checkBsecStatus(envSensor);
  }
  envSensor.attachCallback(newBsecDataCallback);
  Serial.println("BSEC library version " +
                 String(envSensor.version.major) + "." + String(envSensor.version.minor) + "." + String(envSensor.version.major_bugfix) + "." + String(envSensor.version.minor_bugfix));
}

void setup()
{
  // Set up serial
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  Serial.println("Starting...");

  // Set up button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Set up RGB LED
  ledStrip.begin();
  ledStrip.setBrightness(RGB_BRIGHTNESS);
  ledStrip.clear();
  ledStrip.show();

  setupBsec();
}

void checkButton()
{
  if (!digitalRead(BUTTON_PIN))
  {
    tone(BUZZER_PIN, 1000, 100);
    Serial.println("Button down!");
    while (!digitalRead(BUTTON_PIN))
      ;
    tone(BUZZER_PIN, 2000, 100);
    Serial.println("Button released!");
    delay(100);
  }
  pinMode(BUZZER_PIN, INPUT); // Prevent noise from buzzer
}

void loop()
{
  checkButton();
  if (!envSensor.run())
  {
    checkBsecStatus(envSensor);
  }
}
