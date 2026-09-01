# Morse-Code-Transceiver
This a Morse code Transceiver, built using ESP32.

**IoT Dual-Mode Optical Morse Code Transceiver — Technical Connection & System Details**

This project turns an ESP8266 (NodeMCU) into a full-duplex optical Morse communication station. The system supports two operational modes—Transmit (TX) and Receive (RX)—selectable via a hardware button. Transmitted and received data are displayed locally on an OLED display and broadcast simultaneously to a mobile-accessible web dashboard hosted directly by the NodeMCU.

---

**1. Complete Hardware Pin Mapping**

* **OLED Display (0.96" I2C SSD1306):**
* **VCC:** Connects to NodeMCU **3V3**
* **GND:** Connects to NodeMCU **GND**
* **SCL:** Connects to NodeMCU **D1 (GPIO 5)** (I2C Clock line)
* **SDA:** Connects to NodeMCU **D2 (GPIO 4)** (I2C Data line)


* **Tactile Input Switches (4-Legged Buttons):**
* **Dot Button (`.`):** Wired between **D7 (GPIO 13)** and **GND**. Configured in firmware using internal pull-ups (`INPUT_PULLUP`).
* **Dash Button (`-`):** Wired between **D6 (GPIO 12)** and **GND**. Configured using `INPUT_PULLUP`.
* **Mode Switch Button (TX/RX Toggle):** Wired between **D3 (GPIO 0)** and **GND**. Configured using `INPUT_PULLUP`.
* *Wiring Note for 4-Pin Switches:* Connect one leg to the respective GPIO pin and the **diagonally opposite leg** to the GND rail to prevent accidental always-on short circuits.


* **Optical Receiver (LDR Sensor Module):**
* **VCC:** Connects to NodeMCU **3V3**
* **GND:** Connects to NodeMCU **GND**
* **Signal Output (A0 or D0):** Connects to NodeMCU **A0 (ADC0)**. Firmware evaluates incoming light using an analog threshold where readings $< 500$ represent an active optical carrier (Active Low configuration).


* **Transmitter Unit (Laser Diode & Active Buzzer):**
* **Laser Positive (+) & Buzzer Positive (+):** Tied together and connected to **D5 (GPIO 14)**.
* **Laser Negative (-) & Buzzer Negative (-):** Tied together and connected to the common **GND** rail.


* **Decoupling Capacitor:**
* Connect a **100µF electrolytic capacitor** across the breadboard power rails (**Positive to 3V3**, **Negative stripe to GND**) as close to the NodeMCU power pins as possible.



---

**2. Circuit Design & Electrical Considerations**

* **Current Spikes and Decoupling:** Simultaneous operation of the Wi-Fi power amplifier, laser module, and active buzzer creates instantaneous current draw spikes. The 100µF capacitor acts as a local reservoir, eliminating voltage sags (brownout conditions) that would otherwise crash the ESP8266 or disrupt the I2C bus communicating with the OLED.
* **ADC and Wi-Fi Concurrency:** The ESP8266 shares internal analog circuitry between the `A0` ADC and the RF calibration routines. Sampling `A0` continuously inside an unthrottled loop causes Wi-Fi packet drops and AP disconnection. The firmware enforces a 20ms sampling window and cooperative multitasking routines (`yield()` and `delay(1)`) to preserve RF stability during Receive Mode.

---

**3. Operating Logic & Communication Flow**

* **Transmit (TX) Mode:**
* Pressing the Dot button emits a short 150ms optical and audible pulse.
* Pressing the Dash button emits a longer 400ms pulse.
* When 1.5 seconds elapse without an input, the accumulated sequence is decoded into its corresponding ASCII character.
* The decoded character is pushed to the global `sentText` buffer and shown on the OLED.


* **Receive (RX) Mode:**
* The LDR monitors incoming light pulses from a remote transmitter.
* Pulses lasting under 300ms are classified as dots; pulses exceeding 300ms are classified as dashes.
* A 1.5-second dark period triggers letter finalization and appends the decoded character to the `receivedText` buffer.


* **Display & Data Persistence:**
* Toggling the mode switch clears the temporary OLED screen buffer to start a clean display session.
* The complete message history for both TX and RX is preserved in memory and served to any connected browser via the local HTTP server (`192.168.4.1`) hosted under the SSID `Morse_Master_Station`.
