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
NeoGrip communicates with PC via ESP32 using the [LucidVR Driver](https://github.com/LucidVR/opengloves-driver "LucidVR"). While LucidVR is meant for creating your own [Cheap haptic gloves](https://github.com/LucidVR/lucidgloves "Cheap haptic gloves"), NeoGrip takes advantage of the driver's Valve Index controller emulation to send keystrokes to your PC. But here comes a problem. LucidVR driver needs some kind of tracker to determine the position of your hands in 3D space (usually using headsets' original controllers or VIVE trackers). But you probably own none of those since you read this. That is when [ALVR](https://github.com/alvr-org/ALVR "ALVR") comes in clutch. It allows you to emulate VR controllers such as Quest 2 or similar using hand tracking. So basically ALVR tracks your hands instead of a controller or tracker and Steam VR sees it as the controllers you set ALVR to emulate.  SoLucidVR takes care of Buttons and Joysticks and ALVR takes care of tracking.

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
Can be a bit tricky, depending on your setup and you need to tweak settings a lot. Here is what works for me:

### LucidVR setup:
Finish setup according to [their repository](https://github.com/LucidVR/lucidgloves "their repository"). Make sure you close Arduino ide after programming, since it makes the COM port busy and not usable for the driver. Copy [my settings](https://github.com/AthemiS13/NeoGrip/tree/main/Config/LucidVR-Driver "my settings") just change the COM ports of your controllers according to ArduinoIDE.

### ALVR setup:
[Install ALVR](https://github.com/alvr-org/ALVR/wiki/Installation-guide) and then tweak the settings according to [mine](https://github.com/AthemiS13/NeoGrip/tree/main/Config/ALVR "mine"). Make sure ALVR [registered](https://github.com/AthemiS13/NeoGrip/blob/main/Config/ALVR/ALVRSETUP5.png "registered") LucidVR driver. If it is not registered, you will not be able to connect NeoGrip controllers to Steam VR.

### Steam VR setup:
Make sure you have the Lucid VR driver enabled in Steam VR advanced settings. Then adjust button bindings, since the default ones for Valve index controllers do not work properly. When binding in the Steam VR environment, select options above the line. For example, if you want to bind the trigger, you click the plus icon next to the trigger and then select the trigger option above the line.

## Future Plans
- Add Bluetooth communication for wireless functionality.
- Explore additional gesture support and feedback mechanisms.

[![basic](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/basic.png "basic")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/basic.png "basic")

## Demo video
[![Watch the video](https://img.youtube.com/vi/AhS3Zu6njnE/maxresdefault.jpg)](https://youtu.be/AhS3Zu6njnE?si=yEACRPgUvw43rx8U)
