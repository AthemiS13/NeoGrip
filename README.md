# NeoGrip
Affordable custom controllers for Quest 2 and other VR headsets **purchased without original controllers**, or for those looking for open-source VR controllers. Designed using **ESP32** and [ALVR's API](https://github.com/alvr-org/ALVR "ALVR API") to emulate Quest controllers in **SteamVR**, this project makes headsets without controllers usable and accessible for VR enthusiasts on a budget.

> **Note:** The full, comprehensive documentation for this project is located in [Assets/dokumentace.pdf](Assets/dokumentace.pdf). Unfortunately, it is currently only available in the Czech language.

[![NeoGrip animation](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/neogripv2.gif "NeoGrip animation")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/neogripv2.gif "NeoGrip animation")

## Motivation & Goals
The VR equipment market has a paradox: second-hand headsets like Meta Quest are very affordable, but they are often sold without original controllers, which are either unavailable or excessively expensive. NeoGrip was created to break this barrier, utilizing the hidden potential of incomplete VR setups. The goal is to provide a fully functional, open-source hardware and software solution that bridges the physical environment with SteamVR through affordable components.

## Hardware Requirements (Bill of Materials)
- **ESP32** Dev Module [Link](https://www.aliexpress.com/item/1005004879572949.html "Link")
- **TP4056** Charging Module [Link](https://www.aliexpress.com/item/1005007010409267.html "Link")
- **Joystick** KY-023 module [Link](https://www.aliexpress.com/item/1005006966359366.html "Link")
- **Buttons** (A, B, X, Y, Menu, Oculus) [Link](https://www.aliexpress.com/item/1005004254514071.html "Link")
- **End switches** Grab and Trigger (KW11-3Z-4-N) [Link](https://www.aliexpress.com/item/1005006260069918.html "Link")
- 3D-printed controller **parts** (STEP files available [here](https://github.com/AthemiS13/NeoGrip/tree/main/STEP-Files "here"))
 - **Regulator** Ultra Low Dropout voltage, 3.3V (RT9183-33GGF)
 - **Battery** 3.7V, 500mAh, Li-Po (You might be able to fit a bigger one) [Link](https://www.aliexpress.com/item/1005003156469047.html "Link")
  - **Transistor** Generic PNP or NPN transistor for haptic motor control.
  - **Haptic** Motor, 3.3V [Link](https://www.aliexpress.com/item/1005007550657082.html "Link")
  - **Screws, Springs, 2mm Shaft, Wires** 

## Hardware Design & Architecture
NeoGrip is designed with ergonomics and functionality mirroring the Meta Quest 2/3 controllers:
- **Core Controller:** ESP32 Dev Module (Dual-core Xtensa LX6 240MHz, Wi-Fi 2.4GHz) for minimal latency and high performance.
- **Controls:** Standard microswitches (A/X, B/Y, System, Joystick Click), highly reliable end switches KW11-3Z for Trigger and Grip (translated to 0.0/1.0 analog values for OpenXR), and KY-023 analog joysticks for X/Y movement.
- **Haptic Feedback:** Embedded 3.3V vibration motor driven by PWM via a PNP transistor for tactile responses from SteamVR.
- **Power Management:** Powered by a 3.7V 500mAh Li-Po battery providing ~6 hours of active playtime. Protected and charged via a TP4056 module (USB-C or Micro-USB). Includes an automatic deep sleep mode after 60 seconds of inactivity to save power, easily wakeable via the System button. An LDO RT9183 regulator provides stable 3.3V.

## Software Requirements
- [NeoGrip Proxy for Windows](https://github.com/AthemiS13/NeoGrip/releases) (download the latest `NeoGrip-Proxy.exe` release)
- [ALVR](https://github.com/alvr-org/ALVR "ALVR")
- [Arduino IDE](https://www.arduino.cc/en/software "Arduino IDE") or compatible ESP32 programming environment
- SteamVR
### Notes on Quest 2 Setup:
If you have purchased a Quest 2 without controllers that is logged out and factory resetted, bypassing the **initial setup** can be challenging. While this README focuses on the VRController project, instructions for bypassing the setup to enable hand tracking and unlock the headset can be provided on request.

## Software setup
### NeoGrip Proxy setup:
1. Go to the [latest NeoGrip release](https://github.com/AthemiS13/NeoGrip/releases) and download `NeoGrip-Proxy.exe`.
2. Start ALVR on the PC that will run the proxy.
3. Run `NeoGrip-Proxy.exe` and leave its console window open during your VR session.
4. Press the **System** button on each NeoGrip controller to connect it.

The proxy receives controller input on UDP port `9999`, sends haptic feedback on UDP port `8888`, and must be on the same local Wi-Fi network as the controllers.

> Windows SmartScreen may show a warning because the executable is not code-signed. If you downloaded it from the official NeoGrip GitHub release, select **More info** and then **Run anyway**.
### Pin Mapping (Right Controller Example)
- `GPIO 4`: System Button / Deep Sleep Wake (RTC GPIO)
- `GPIO 14`: Joystick Click
- `GPIO 16`: PWM Motor Output
- `GPIO 18`: B Button
- `GPIO 19`: Trigger (KW11-3Z)
- `GPIO 21`: A Button
- `GPIO 27`: Squeeze/Grip (KW11-3Z)
- `GPIO 34`: Joystick X-Axis (ADC 0-4095)
- `GPIO 35`: Joystick Y-Axis (ADC 0-4095)

### 3D Printed Chassis
Designed in Autodesk Fusion 360, printable via FDM. Recommended settings:
- **Material:** PLA (PETG optional for more durable triggers).
- **Layer Height:** 0.2mm for the main body, 0.15mm for finer button details.
- **Infill:** 65% Gyroid for high structural integrity and low weight.
- **Temperature:** 210°C for optimal layer adhesion.
- **Supports:** Tree-type supports recommended.

[![Side](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/v2side.png "Side")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/v2side.png "Side")

## How it Works
The architecture consists of the ESP32 hardware and a multi-threaded Python-based proxy server on the PC.

### 1. ESP32 Input Processing
Every 40ms, the ESP32 reads analog and digital inputs and formats them into a lightweight 15-character UDP broadcast packet (e.g., `L00010204720470`):
- **Char 1:** Side identifier (`L` or `R`).
- **Chars 2-6:** Button states (`A/X`, `B/Y`, `SYS`, `Trigger`, `Grip`) as `0` or `1`.
- **Chars 7-10:** Joystick X-axis analog value (`0000`-`4095`).
- **Chars 11-14:** Joystick Y-axis analog value (`0000`-`4095`).
- **Char 15:** Joystick Click state (`0` or `1`).

### 2. Python Proxy Server (`NeoGrip.py`)
To prevent blocking, the proxy server uses multiple threads:
- **Main Thread:** Receives UDP packets from the ESP32 on port `9999`.
- **ALVR Thread:** Forwards processed controller data to ALVR's REST API (`POST http://127.0.0.1:8082/api/set-buttons`).
- **WebSocket Thread:** Listens to `ws://localhost:8082/api/events` for haptic feedback triggers from SteamVR.
- **Start Signal Thread:** Broadcasts a "START" UDP packet to wake controllers from deep sleep.

### 3. ALVR and SteamVR Integration
The proxy translates raw values into standardized OpenXR paths (e.g., `/user/hand/left/input/trigger/value`), allowing the controllers to work natively with SteamVR. Instead of built-in positional tracking, the project relies on **Meta Quest Hand Tracking**. SteamVR binds the controller inputs directly onto the tracked hands in VR.

### 4. Haptic Feedback
The WebSocket thread captures haptic events, parsing the target controller (path), vibration duration, and amplitude. It maps amplitude (0.0-1.0) to PWM duty cycles (0-255) and converts the duration to milliseconds. It then sends a 7-character UDP packet back to the ESP32 on port `8888` (e.g., `R255100` -> Right controller, 255 duty cycle, 100ms duration).

### ALVR Configuration
For ALVR to accept inputs from the proxy server and track the controllers properly via headset cameras, you must adjust the following settings in ALVR:
1. **Presets tab:** Turn **off** "Hand tracking interaction" (we want to use the controller buttons, not gesture interactions).
2. **Headset tab:** Ensure the following are **enabled/on**:
   - `Controllers`
   - `Tracked`
   - `Multimodal tracking`
   - `Haptics`
3. Ensure that "SteamVR Input 2.0" and "Hand tracking interaction" (under Headset) are disabled.

> **Note:** A detailed visual guide for the ALVR configuration, alongside screenshots, is available in the [documentation PDF](Assets/dokumentace.pdf) as well as in the [Config](Config) directory.

### Running the System
1. Start SteamVR and ALVR. Verify configuration.
2. Run the proxy server via terminal:
   ```bash
   python NeoGrip.py
   ```
   The terminal will indicate that it's broadcasting the "START" signal.
3. Press the **System** button on your NeoGrip controllers to wake them up. They will connect to Wi-Fi and vibrate briefly upon a successful connection.
4. Put on your headset. Your controllers are now ready to use in SteamVR!
5. To stop, press `Ctrl+C` in the proxy console. The proxy will send a "STOP" signal, putting the controllers back to deep sleep.

## Performance & Testing
- **Latency:** The system provides extremely low latency, averaging under 50ms from physical button press to VR action, which is well within comfortable limits for fast-paced VR games.
- **Reliability:** The 2.4GHz Wi-Fi UDP connection provides robust data transmission with negligible packet loss (<1%). Missing packets are instantly superseded by the 25Hz refresh rate.
- **Battery Life:** ~6 hours of continuous playtime. The TP4056 module charges the 500mAh Li-Po battery at a rate of 1A, allowing a full recharge from 0 to 100% in approximately 45 minutes.

## Videos

### Finished Product Showcase
[**Watch the NeoGrip Finished Product Showcase on YouTube**](https://www.youtube.com/watch?v=nmFFR0dLkeE)

[![Watch the NeoGrip Finished Product Showcase](https://img.youtube.com/vi/nmFFR0dLkeE/maxresdefault.jpg)](https://www.youtube.com/watch?v=nmFFR0dLkeE)

### 2024 Demo
[**Watch the NeoGrip 2024 Demo on YouTube**](https://youtu.be/AhS3Zu6njnE?si=yEACRPgUvw43rx8U)
[![Watch the video](https://img.youtube.com/vi/AhS3Zu6njnE/maxresdefault.jpg)](https://youtu.be/AhS3Zu6njnE?si=yEACRPgUvw43rx8U)
