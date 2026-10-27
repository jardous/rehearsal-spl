# Wiring and power

Verified against official Adafruit documentation and the Arduino board variant
on 2026-10-27. Power off before wiring.

| ICS-43434 breakout (6049) | MatrixPortal S3 (5778) | Direction |
|---|---|---|
| 3V | 3V | regulated 3.3 V to mic |
| GND | GND | common ground |
| BCLK | A2 / GPIO9 | S3 → mic |
| LRCLK | A3 / GPIO10 | S3 → mic |
| DOUT | A4 / GPIO11 | mic → S3 |
| SEL | GND | transmit during left / LRCLK low |

**Do not connect the microphone to 5 V or use 5 V I2S signals.** Keep wires
short (preferably below 10–15 cm), with nearby ground conductors. Keep the
microphone's bottom acoustic port unobstructed and away from panel vibration,
airflow, power wiring and surfaces that create strong acoustic reflections.
The similarly shaped SPH0645 is not an interchangeable calibrated microphone:
its sensitivity and digital timing differ. This firmware targets ICS-43434 only.

## Matrix

Connect MatrixPortal directly to the matrix **HUB75 input**, carefully aligned.
The assumed panel is a conventional 64×32, 1:16-scan HUB75 panel. Exotic scan
patterns or panels needing special driver initialization are not supported.
Use the matrix power harness from MatrixPortal **+5V/GND screw outputs** to
the panel. HUB75 does not supply panel power. Address-E solder jumpers are not
needed for a 32-pixel-high panel.

The official Protomatter S3 example uses:

| Function | GPIOs |
|---|---|
| R1/G1/B1/R2/G2/B2 | 42 / 41 / 40 / 38 / 39 / 37 |
| A/B/C/D | 45 / 36 / 48 / 35 |
| CLK / LAT / OE | 2 / 47 / 14 |
| UP / DOWN | 6 / 7, active low, internal pullups enabled |

No HUB75 or button signal overlaps GPIO9/10/11. Protomatter uses LCD_CAM,
GDMA TX and hardware timer 0; microphone uses I2S0 and a GDMA RX channel.
Both DMA buffers use internal RAM, not PSRAM. The shared DMA engine and memory
bus still require real-hardware load testing; separate peripherals are not
proof of gap-free operation.

## USB-C power

Use a reputable **regulated 5 V USB-C supply**, a suitably rated cable and
adequate current for your particular panel. A 64×32 matrix at full-white can
draw several amps; a computer USB port is not a suitable guaranteed runtime
power source. Sparse graphics and default brightness 64 reduce average draw,
but software brightness is **not a current limiter or a power negotiation
mechanism**. Start dim, check the panel's rating and supply/cable temperature.
Do not assume an advertised 3 A supply is sufficient for every panel or image.

Adafruit labels the screw terminals **outputs only**. Do not back-feed them,
especially with USB connected. If your panel needs a separately powered supply,
disconnect its +5 V from MatrixPortal, power the panel directly and keep grounds
common, following Adafruit's guidance. Do not connect two 5 V sources together.
The requested single-USB-C setup is viable only within its supply, cable and
board power-path limits.

## Official sources

- [MatrixPortal S3 pinouts, buttons and power](https://learn.adafruit.com/adafruit-matrixportal-s3/pinouts)
- [MatrixPortal S3 Arduino board variant, core 2.0.17](https://github.com/espressif/arduino-esp32/blob/2.0.17/variants/adafruit_matrixportal_esp32s3/pins_arduino.h)
- [Adafruit I2S microphone pinouts, including ICS-43434](https://learn.adafruit.com/adafruit-i2s-mems-microphone-breakout/pinouts)
- [ICS-43434 product 6049 / nominal sensitivity and specifications](https://www.adafruit.com/product/6049)
- [ICS-43434 datasheet v1.2](https://cdn-shop.adafruit.com/product-files/6049/6049_DS-000069-ICS-43434-v1.2.pdf)
- [Protomatter simple example, pinned version](https://github.com/adafruit/Adafruit_Protomatter/blob/1.7.0/examples/simple/simple.ino)
- [Protomatter S3 peripheral implementation](https://github.com/adafruit/Adafruit_Protomatter/blob/1.7.0/src/arch/esp32-s3.h)
