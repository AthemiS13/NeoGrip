# NeoGrip
> **Beta — firmware and proxy rewrite:** This branch contains substantial changes that have not been fully verified on hardware or with ALVR and SteamVR. It is available for testing and feedback; keep a copy of your working firmware and proxy before trying it. Use the firmware and proxy from this branch together. Existing release executables may not include these changes; run the beta proxy from source using the instructions below.

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
- **Haptic Feedback:** Embedded 3.3V vibration motor driven by PWM through a transistor for tactile responses from SteamVR.
- **Power Management:** Powered by a 3.7V 500mAh Li-Po battery providing ~6 hours of active playtime. Protected and charged via a TP4056 module (USB-C or Micro-USB). Enters standby after 60 seconds without a proxy connection while connected to Wi-Fi, or when the proxy closes, and wakes with the System button. An LDO RT9183 regulator provides stable 3.3V.

## Software Requirements
- [NeoGrip Proxy for Windows](https://github.com/AthemiS13/NeoGrip/releases) (download the latest `NeoGrip-Proxy.exe` release)
- [ALVR](https://github.com/alvr-org/ALVR "ALVR")
- [Arduino IDE](https://www.arduino.cc/en/software "Arduino IDE") or compatible ESP32 programming environment
- SteamVR
### Notes on Quest 2 Setup:
If you have purchased a Quest 2 without controllers that is logged out and factory resetted, bypassing the **initial setup** can be challenging. While this README focuses on the VRController project, instructions for bypassing the setup to enable hand tracking and unlock the headset can be provided on request.

## Software setup

### Controller firmware

1. Install **Arduino-ESP32 3.x** in Arduino IDE and select **ESP32 Dev Module**.
2. Open `VR-Firmware/ESP32/NeoGrip/NeoGrip.ino` in Arduino IDE. Keep `WiFiSetup.h`, `MotorFeedback.h` and `MotorFeedback.cpp` in the same sketch folder.
3. Set `controllerType` to `"L"` for the left controller or `"R"` for the right, check the GPIO assignments for your build, and upload to each controller.

Set `motorActiveLow` for each controller's motor driver wiring, independently of its left/right setting: use `true` for active-low control (typically PNP), or `false` for active-high control (typically NPN). The motor should be silent at idle.

### Wi-Fi setup from your phone

1. Power on a controller. On first use, it creates a Wi-Fi network named **`NeoGrip-L-xxxxxx`** or **`NeoGrip-R-xxxxxx`**.
2. Connect your phone to that network using password **`neogripsetup`**. If your phone reports no internet, stay connected.
3. Open the setup page that appears, or visit **http://192.168.4.1** in your browser.
4. Enter the name and password of the **2.4 GHz Wi-Fi network shared with your VR computer**, then select **Connect and save**. Repeat for the other controller.

The controller remembers the network after a successful connection. An incorrect password leaves the previous saved network intact. After connecting, the setup access point closes in about 15 seconds; reconnect your phone to your normal Wi-Fi. If setup is interrupted while the controller joins the router, rejoin its NeoGrip network and reopen the page.

To change networks or update a password later, power on the controller where its saved network is unavailable. After about **20 seconds** without a connection, the setup network appears again. The controller also keeps trying its saved network, so a temporary router outage does not require reconfiguration.

### NeoGrip Proxy setup:
1. Go to the [latest NeoGrip release](https://github.com/AthemiS13/NeoGrip/releases) and download `NeoGrip-Proxy.exe`.
2. Start ALVR on the PC that will run the proxy.
3. Run `NeoGrip-Proxy.exe` and leave its console window open during your VR session.
4. Press the **System** button on each NeoGrip controller to connect it.

The proxy receives controller input on UDP port `9999`, sends haptic feedback on UDP port `8888`, and must be on the same local Wi-Fi network as the controllers. Update the proxy together with the firmware so either controller can reconnect after restarting.

To run the current proxy from source, install Python and run these commands from the repository folder:

```bash
python -m pip install -r requirements.txt
python VR-Firmware/NeoGrip-Proxy/NeoGrip.py
```

If your joystick drifts near the center, add `--deadzone 0.08` to the proxy command. Use `--debug` for connection and haptic diagnostics, or `--help` for ALVR address and network options.

> Windows SmartScreen may show a warning because the executable is not code-signed. If you downloaded it from the official NeoGrip GitHub release, select **More info** and then **Run anyway**.

### Wireless uploads from Arduino IDE

Flash this OTA-enabled firmware to each controller **once by USB**. Use an ESP32 partition scheme with two OTA app slots, such as **Default 4MB with spiffs**; schemes labelled **No OTA** or **Huge APP** cannot support wireless firmware updates.

At the initial USB setup, open Serial Monitor at **115200 baud** and record the controller's `[OTA]` upload password. Each device generates and saves its own password across updates. If you missed the boot message, leave Serial Monitor open and hold **menu + trigger for seven seconds** to restart the controller. You can instead set `otaPassword` in the sketch to an 8-64 byte password before flashing; that password is also saved for future builds that leave the setting empty.

For later updates:

1. Connect the computer and controller to the same Wi-Fi. Keep the proxy running so the controller stays awake while Arduino IDE compiles.
2. In **Tools → Port**, select the controller's **network port**, named `NeoGrip-L-xxxxxx` or `NeoGrip-R-xxxxxx`.
3. Keep the correct `controllerType`, GPIO assignments and `motorActiveLow` setting for that controller, then click **Upload** and enter its saved OTA password when prompted.
4. Arduino IDE shows upload progress. The motor stays off during flashing; the controller restarts after a successful update and reconnects to the proxy automatically. Upload to each controller separately.

OTA is available while the controller is awake on its saved Wi-Fi. It also works on the NeoGrip setup access point: connect the computer to that network and select the controller at `192.168.4.1` in Arduino IDE. The normal sleep behaviour still applies when the proxy is closed. If a network port is missing, wake the controller with **System** and allow a few seconds for discovery; the local firewall must allow Arduino IDE's ESP32 network uploader. A failed or interrupted upload does not select the incomplete image for boot, and the old firmware can reconnect for another attempt. Saved Wi-Fi credentials and the device's OTA password are retained.

### Restarting and reconnecting

- Hold **menu/system + trigger together for seven seconds** to restart only that controller. It plays three quick pulses before restarting. Releasing either button cancels the hold. Release both after restarting before using the gesture again.
- Leave the proxy running: controllers reconnect automatically after a restart or temporary Wi-Fi interruption. Input is sent directly to the discovered proxy. The proxy acknowledges received input so a controller can repair a stalled sending path even when discovery heartbeats still arrive. A quiet control listener is reopened without interrupting input; persistent sending failures trigger a Wi-Fi reconnect. A watchdog also restarts the controller if its firmware stops responding.
- Press **System** to wake a sleeping controller. It enters standby after 60 seconds without a proxy connection while connected to Wi-Fi, or when you close the proxy with `Ctrl+C`. It stays awake while Wi-Fi setup is open.

### Vibration notifications

The controller uses distinct vibration patterns to report its state. Pulses have shaped strength changes and pauses so they are recognizable without a display.

| Event | Pattern |
| --- | --- |
| Power on or wake | Two rising pulses |
| Wi-Fi connected | One medium pulse |
| Proxy connected or restored | Two quick pulses followed by a longer confirmation |
| Wi-Fi connection lost | Three slower pulses |
| Proxy unavailable or input sending interrupted | Two long pulses |
| Phone Wi-Fi setup ready | Long, short, long |
| Entering standby | Two descending pulses |
| Restart gesture accepted | Three quick pulses |

Status cues play once per event. Wi-Fi loss is announced after three seconds; an unavailable proxy is announced after five seconds. Game haptics resume after status feedback. Shutdown feedback takes priority and cannot be extended by incoming game effects.

Game effects keep their requested duration, including sustained vibration for automatic weapons. There is no continuous-play time cap or forced cooldown. For effects longer than 999 ms, the proxy renews short transport segments every 250 ms without inserting gaps. Each segment expires independently in the firmware, so a lost connection or stalled proxy cannot leave an old effect running for more than about one second after its last packet. A firmware timer also switches the motor off when the current segment expires if the main loop stalls. Closing the proxy or losing its ALVR WebSocket cancels pending renewals.

These checks protect against stale commands and stuck software output. The controller does not measure motor or transistor temperature; valid ongoing game commands cannot be distinguished from excessive heating by timing alone.

Standby keeps the motor output at its configured off level. With GPIO16 or GPIO17, the controller uses light sleep with the pin supply retained; this consumes more standby power than deep sleep. Press **System** to wake it normally.

### Pin Mapping (Right Controller Example)
- `GPIO 4`: System Button / Wake (RTC GPIO)
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
Every 40ms (25 Hz), the ESP32 reads analog and digital inputs and sends them to the proxy in a lightweight 15-character UDP packet (e.g., `L00010204720470`):
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
- **Haptics Thread:** Renews long effects without interrupting input forwarding or adding delay to short effects.
- **Start Signal Thread:** Keeps discovering controllers and sending heartbeats throughout the session, allowing either hand to reconnect independently.

### 3. ALVR and SteamVR Integration
The proxy translates raw values into standardized OpenXR paths (e.g., `/user/hand/left/input/trigger/value`), allowing the controllers to work natively with SteamVR. Instead of built-in positional tracking, the project relies on **Meta Quest Hand Tracking**. SteamVR binds the controller inputs directly onto the tracked hands in VR.

### 4. Haptic Feedback
The WebSocket thread captures haptic events, parsing the target controller (path), vibration duration, and amplitude. It maps amplitude (0.0-1.0) to motor strength (0-255) and converts the duration to milliseconds. It sends long effects as overlapping, expiring segments and short effects immediately, using a 7-character UDP packet directly to the target controller on port `8888` (e.g., `R255100` -> right controller, full strength, 100ms duration). The firmware applies the configured motor polarity and handles pulse timing without blocking input updates.

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
   Keep the proxy running throughout your VR session; it continues discovering controllers automatically.
3. Press the **System** button on your NeoGrip controllers to wake them up. They will join their saved Wi-Fi network and vibrate briefly when the proxy connects. On first use, complete the phone Wi-Fi setup above.
4. Put on your headset. Your controllers are now ready to use in SteamVR!
5. To stop, press `Ctrl+C` in the proxy console. The proxy will stop haptics and send a "STOP" signal. Each controller plays its sleep cue and enters standby after the System button is released.

## Performance & Testing
- **Latency:** Controllers send input at 25 Hz (every 40ms). The proxy combines pending updates from both hands, caps HTTP traffic and drops expired input to keep controls responsive. Total latency depends on Wi-Fi and ALVR.
- **Reliability:** Controllers recover automatically from temporary Wi-Fi loss and can reconnect independently. If a controller disconnects, the proxy releases its held controls to prevent a stuck trigger or grip.
- **Battery Life:** Runtime depends on Wi-Fi conditions and haptic use. Wi-Fi power saving is disabled for responsiveness; leaving the Wi-Fi setup network open also uses more power. The TP4056 module charges the 500mAh Li-Po battery at a rate of 1A, allowing a full recharge from 0 to 100% in approximately 45 minutes.

## Videos

### Finished Product Showcase
[**Watch the NeoGrip Finished Product Showcase on YouTube**](https://www.youtube.com/watch?v=nmFFR0dLkeE)

[![Watch the NeoGrip Finished Product Showcase](https://img.youtube.com/vi/nmFFR0dLkeE/maxresdefault.jpg)](https://www.youtube.com/watch?v=nmFFR0dLkeE)

### 2024 Demo
[**Watch the NeoGrip 2024 Demo on YouTube**](https://youtu.be/AhS3Zu6njnE?si=yEACRPgUvw43rx8U)
[![Watch the video](https://img.youtube.com/vi/AhS3Zu6njnE/maxresdefault.jpg)](https://youtu.be/AhS3Zu6njnE?si=yEACRPgUvw43rx8U)
