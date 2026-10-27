// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <Adafruit_Protomatter.h>

// Adafruit's official MatrixPortal S3 simple example pin assignment.
static uint8_t rgb[] = {42, 41, 40, 38, 39, 37};
static uint8_t addr[] = {45, 36, 48, 35};
static Adafruit_Protomatter matrix(64, 4, 1, rgb, 4, addr, 2, 47, 14, true);

void setup() {
  Serial.begin(115200);
  auto status = matrix.begin();
  Serial.printf("Protomatter status=%d\n", status);
  if (status != PROTOMATTER_OK) return;
  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  matrix.setTextColor(matrix.color565(0, 64, 0));
  matrix.setTextSize(3);
  matrix.setCursor(0, 0);
  matrix.print("94");
  matrix.setTextSize(1);
  matrix.setCursor(38, 13);
  matrix.print("dBA");
  matrix.show();
}
void loop() { delay(100); }
