# NeoGrip
Affordable custom controllers for Quest 2 and other VR headsets **purchased without original controllers**, or for those looking for open-source VR controllers. Designed using **ESP32** and [LucidVR Driver](https://github.com/LucidVR/opengloves-driver "LucidVR Driver") to emulate Valve Index controllers in **SteamVR**, this project makes headsets without controllers usable and accessible for VR enthusiasts on a budget.

[![NeoGrip animation](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/animation.gif "NeoGrip animation")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/animation.gif "NeoGrip animation")

## Update!!
[![NeoGrip animation](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/neogripv2.gif "NeoGrip animation")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/neogripv2.gif "NeoGrip animation")

- **Major design update:** V1 was almost impossible to use comfortably because of its size and non-ergonomic design. V2 brings a completely new design that has already been 3D printed and tested and works flawlessly.
- **Software update:** NeoGrip is slowly getting rid of [LucidVR Driver](https://github.com/LucidVR/opengloves-driver "LucidVR Driver") because more flexible, lightweight and simple solution is needed. I am currently collaborating with [ALVR's](https://github.com/alvr-org/ALVR "ALVR") Developers to build NeoGrip's firmware based on ALVR's API.
- **This will bring:** Wireless Wi-Fi-based communication, Haptic feedback, and possibly Hall effect sensors (currently using binary state of presses)12
- 5.12. 2025: NeoGrip no longer uses LucidVR Driver. I worked hard with ALVR devs and finally brought NeoGrip to life as I wanted. It uses a proxy written in python to handle ESP to PC communication. I will add more info once it is completely done.
  
## Features
- **Custom Controller Design:** Includes joystick, trigger, grab button, and three additional buttons. You can customize everything.
- **Affordable Solution:** Ideal for users who bought Quest 2 headsets without controllers for cost savings.
- **Connectivity:** USB-C communication (Bluetooth support planned).
- **Driver:** Uses the [LucidVR Driver](https://github.com/LucidVR/opengloves-driver "LucidVR Driver") for Valve Index emulation.
- **Hardware:** Built with ESP32 microcontroller and designed for DIY assembly.

## Motivation
Quest 2 headsets without controllers are often significantly **cheaper** but practically unusable due to setup barriers and lack of interaction devices. This project provides an affordable, open-source solution for people in this situation, turning these **undervalued** headsets into functional VR systems.

## How does it work?
NeoGrip communicates with PC via ESP32 using the [LucidVR Driver](https://github.com/LucidVR/opengloves-driver "LucidVR"). While LucidVR is meant for creating your own [Cheap haptic gloves](https://github.com/LucidVR/lucidgloves "Cheap haptic gloves"), NeoGrip takes advantage of the driver's Valve Index controller emulation to send keystrokes to your PC. But here comes a problem. LucidVR driver needs some kind of tracker to determine the position of your hands in 3D space (usually using headsets' original controllers or VIVE trackers). But you probably own none of those since you read this. That is when [ALVR](https://github.com/alvr-org/ALVR "ALVR") comes in clutch. It allows you to emulate VR controllers such as Quest 2 or similar using hand tracking. So basically ALVR tracks your hands instead of a controller or tracker and Steam VR sees it as the controllers you set ALVR to emulate.  SoLucidVR takes care of Buttons and Joysticks and ALVR takes care of tracking.

## Hardware Requirements
- **ESP32** microcontroller
- **USB-C** cable
- **Joystick** module
- 3 **buttons** (A, B, X, Y, Menu, Oculus)
- **End switches** (for grab button functionality)
- 3D-printed controller **shell** (design files available [here](https://github.com/AthemiS13/NeoGrip/tree/main/3D-Files "here"))

[![Side](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/side.png "Side")](https://github.com/AthemiS13/NeoGrip/blob/main/Assets/side.png "Side")

## Software Requirements
- [LucidVR Driver](https://github.com/LucidVR/opengloves-driver "LucidVR Driver")
- [ALVR](https://github.com/alvr-org/ALVR "ALVR")
- [Arduino IDE](https://www.arduino.cc/en/software "Arduino IDE") or compatible ESP32 programming environment
- SteamVR

## Assembly
1. 3D Print the Shell: Download the [3D Files](https://github.com/AthemiS13/NeoGrip/tree/main/3D-Files "here") and print them using your preferred 3D printer.
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
