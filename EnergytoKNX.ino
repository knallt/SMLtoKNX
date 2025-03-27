// SML-Payload direkt mit fixem Offset auslesen – Produktion ohne Debug-Ausgabe
#include <KnxTpUart.h>
#include <SoftwareSerial.h>

#define SML_MSG_BUFFER_SIZE 600
#define SERIAL_READ_TIMEOUT_MS 100
#define RX_PIN 9
#define TX_PIN 8
#define KNX_ADDRESS "1.1.25"
#define TOTAL_CONSUMPTION_ADDRESS "0/6/1"
#define TOTAL_GENERATION_ADDRESS  "0/6/2"
#define POWER_L1L2L3_ADDRESS       "0/6/3"
#define ALIVE_ADDRESS              "0/6/4"
#define ALIVE_INTERVAL 10000

unsigned char buffer[SML_MSG_BUFFER_SIZE];
unsigned long alivePreviousMillis = 0;
SoftwareSerial mySerial(RX_PIN, TX_PIN);
KnxTpUart knx(&Serial1, KNX_ADDRESS);

void setup() {
  Serial1.begin(19200, SERIAL_8E1);
  knx.uartReset();
  mySerial.begin(9600);
  Serial.begin(9600);
  pinMode(17, OUTPUT);
  Serial.println("Setup done");
}

void loop() {
  unsigned long now = millis();

  if (now - alivePreviousMillis >= ALIVE_INTERVAL) {
    alivePreviousMillis = now;
    knx.groupWriteBool(ALIVE_ADDRESS, true);
    Serial.println("Alive signal sent");
  }

  if (!mySerial.available()) return;

  memset(buffer, 0, sizeof(buffer));
  unsigned int pos = 0;
  unsigned long lastRead = millis();

  while (millis() - lastRead < SERIAL_READ_TIMEOUT_MS) {
    if (mySerial.available()) {
      buffer[pos++] = mySerial.read();
      lastRead = millis();
      if (pos >= SML_MSG_BUFFER_SIZE) return;
    }
  }

  if (!isValidHeader()) {
    Serial.println("Invalid SML header");
    return;
  }

  digitalWrite(17, HIGH);

  float totalConsumption = extractRawValue(156) / 10000.0;
  float totalGeneration  = extractRawValue(180) / 10000.0;
  float powerTotal       = extractRawValue(284) / 10.0; // Skalierung von 0.1W auf W

  knx.groupWrite4ByteFloat(TOTAL_CONSUMPTION_ADDRESS, totalConsumption);
  knx.groupWrite4ByteFloat(TOTAL_GENERATION_ADDRESS, totalGeneration);
  knx.groupWrite4ByteFloat(POWER_L1L2L3_ADDRESS, powerTotal);

  Serial.print("Consumption: "); Serial.print(totalConsumption); Serial.print(" kWh, ");
  Serial.print("Generation: "); Serial.print(totalGeneration); Serial.print(" kWh, ");
  Serial.print("Power: "); Serial.print(powerTotal); Serial.println(" W");

  digitalWrite(17, LOW);
}

bool isValidHeader() {
  return buffer[0] == 0x1b && buffer[1] == 0x1b &&
         buffer[2] == 0x1b && buffer[3] == 0x1b &&
         buffer[4] == 0x01 && buffer[5] == 0x01 &&
         buffer[6] == 0x01 && buffer[7] == 0x01;
}

unsigned long extractRawValue(int startIndex) {
  unsigned long val = 0;
  for (int i = 0; i < 4; i++) {
    val = (val << 8) | buffer[startIndex + i];
  }
  return val;
}

// Optional: Debug-Funktionen können bei Bedarf wieder aktiviert werden
void dumpHexBuffer() {
  Serial.println("--- HEX DUMP START ---");
  for (int i = 0; i < SML_MSG_BUFFER_SIZE; i++) {
    if (buffer[i] < 0x10) Serial.print("0");
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
    if ((i + 1) % 16 == 0) Serial.println();
  }
  Serial.println("--- HEX DUMP END ---");
}

void scanPowerLikeValues() {
  Serial.println("--- SCAN FOR POWER-LIKE VALUES ---");
  for (int i = 20; i < 300; i++) {
    unsigned long val = extractRawValue(i);
    if (val > 50 && val < 15000) {
      Serial.print("Offset ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(val);
    }
  }
  Serial.println("--- SCAN DONE ---");
}
