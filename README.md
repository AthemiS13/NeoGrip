# NeoGrip
Affordable custom controllers for Quest 2 and other VR headsets **purchased without original controllers**, or for those looking for open-source VR controllers. Designed using **ESP32** and [ALVR's API](https://github.com/alvr-org/ALVR "ALVR API") to emulate Quest controllers in **SteamVR**, this project makes headsets without controllers usable and accessible for VR enthusiasts on a budget.

[![NeoGrip animation](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/neogripv2.gif "NeoGrip animation")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/neogripv2.gif "NeoGrip animation")


  
## Features
- **Design:** NeoGrip was designed based on Quest 3's controllers and shares all the controls
- **Controls:** Trigger, Grab, A, B, System, JoyStick Click, JoyStick X and Y analog axis
- **Connectivity:** Wi-Fi via UDP
- **Core:** ESP32 Dev Module
 - **Power:** 500mAh Li-Po battery, TP4056 for charging and protection (Available both with USB-C or Micro), Deep Sleep mode if not in use and can be woken up by system button any time, Up to 6 hours of playtime

## Motivation
Quest 2 headsets without controllers are often significantly **cheaper** but practically unusable due to setup barriers and lack of interaction devices. This project provides an affordable, open-source solution for people in this situation, turning these **undervalued** headsets into functional VR systems.

# !!THIS REPO IS STILL UNDER CONSTRUCTION!!
It still contains old files and outdated informations. If you have any questions, feel free to contact me personally. Hopefully I will fully finish this repo till the end of March. I also want to make some YouTube videos showcasing the functionality and maybe even the assembly process.


## How does it work?

**NeoGrip** is a custom 3D-printed VR controller powered by an ESP32. It connects to your local Wi-Fi network and streams real-time input data to a PC at a consistent 40ms interval. Here’s a breakdown of its functionality:

### Controller Input

Every 40ms, the controller sends a **15-character string** via UDP to the NeoGrip Python proxy running on the PC:

-   **1st character**: Controller side — `L` (Left) or `R` (Right).
    
-   **Next 5 characters**: Button states:
    
    -   A/X, B/Y
        
    -   System button
        
    -   Trigger
        
    -   Grab/squeeze button  
        _(Each represented as `0` or `1`)_
        
-   **Next 8 characters**: Joystick analog values:
    
    -   4 digits for **X-axis** (0000–4095)
        
    -   4 digits for **Y-axis** (0000–4095)
        
-   **Last character**: Joystick click state (`0` or `1`)
    

This data is broadcast to port **9999** and received by the Python proxy.

### Python Proxy

The Python proxy listens for these packets, parses them, and translates them into **ALVR API-compatible keystrokes** for VR interaction. Each input is converted into ALVR’s controller paths for actions like `trigger`, `squeeze`, `thumbstick`, and button clicks.

### Haptic Feedback

Haptic feedback is handled by the Python proxy and sent back to the ESP32 via UDP on **port 8888**. When a haptic event is triggered (e.g., through ALVR’s WebSocket event stream), the proxy sends a **7-character message** to the appropriate controller:

-   **1st character**: Controller side (`L` or `R`)
    
-   **Next 3 digits**: Duty cycle for motor speed (0–255)
    
-   **Next 3 digits**: Duration in milliseconds (max 999ms)
    

Example: `R200150` means "Right controller, 200 PWM, for 150ms".

On the ESP32 side, the motor is controlled via PWM, and the haptic vibration ends automatically after the specified duration.

### Power Management

NeoGrip uses **deep sleep mode** to save power when inactive:

-   When first powered on, the controller waits for a `"START"` signal from the PC. If not received within 60 seconds, it enters deep sleep.
    
-   While sleeping, the controller can be woken up by pressing the **System** button (configured as a hardware wake-up source).
    
-   Upon shutdown (e.g., PC disconnects or ALVR is closed), a `"STOP"` signal is sent to the controller, which also triggers deep sleep mode.
    

This system ensures the controller remains efficient and only stays active during sessions.
## Hardware Requirements
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
 
 

[![Side](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/v2side.png "Side")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/side.png "Side")

## Software Requirements
- [NeoGrip Proxy](https://github.com/AthemiS13/NeoGrip/tree/main/VR-Firmware/NeoGrip-Proxy "NeoGrip Proxy")
- [ALVR](https://github.com/alvr-org/ALVR "ALVR")
- [Arduino IDE](https://www.arduino.cc/en/software "Arduino IDE") or compatible ESP32 programming environment
- SteamVR
- Visual Studio Code or other enviroment to run NeoGrip Proxy
- [Python](https://www.python.org/ "Python")

## Assembly
1. 3D Print the Shell: Download the [3D Files](https://github.com/AthemiS13/NeoGrip/tree/main/STEP-Files "3D files") and print them using your preferred 3D printer.
2. Wire Components: (Connect components to ESP32 According to **NeoGrip** firmware setup)
3. Download [Firmware](https://github.com/AthemiS13/NeoGrip/tree/main/VR-Firmware "Firmware") and open lucidgloves_firmware-left/right **.INO** file in **Arduino IDE**
4. Program the **ESP32** using Arduino IDE.
5. Set up the [LucidVR](https://github.com/LucidVR/opengloves-driver "LucidVR") SteamVR driver. If you have issues refer to their **Discord**.
6. Connect the controller via **USB-C** to ensure proper communication with SteamVR.
7. Setup **button bindings** in Steam VR (Default bindings do not work)

[![Assembly](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/electro.png "Assembly")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/electro.png "Assembly")

### Notes on Quest 2 Setup:
If you have purchased a Quest 2 without controllers that is logged out and factory resetted, bypassing the **initial setup** can be challenging. While this README focuses on the VRController project, instructions for bypassing the setup to enable hand tracking and unlock the headset can be provided on request.

## Software setup
### NeoGrip Proxy setup:
1. Install leatest [Python](https://www.python.org/downloads/) version
2. Download [NeoGrip Proxy](https://github.com/AthemiS13/NeoGrip/tree/main/VR-Firmware/NeoGrip-Proxy)
3. Open Python file in Visual Studio Code or simmilar editor


### ALVR setup:
[Install ALVR](https://github.com/alvr-org/ALVR/wiki/Installation-guide) and then tweak the settings according to [mine](https://github.com/AthemiS13/NeoGrip/tree/main/Config/ALVR "mine"). 


[![basic](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/basic.png "basic")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/basic.png "basic")

## Demo video
[![Watch the video](https://img.youtube.com/vi/AhS3Zu6njnE/maxresdefault.jpg)](https://youtu.be/AhS3Zu6njnE?si=yEACRPgUvw43rx8U)
