# STM32 Firmware Over-The-Air (FOTA) Update

This project implements a Firmware Over-The-Air (FOTA) update mechanism for STM32 microcontrollers using a custom bootloader. The STM32 communicates with a host (Node-RED) via ESP32 to receive commands from a Node-RED Dashboard. The firmware update file is stored in Firebase and delivered to the STM32 for seamless updates.

---

### Video link: https://youtu.be/6jXGq0C-o00
![9](https://github.com/user-attachments/assets/6341730e-08d1-4543-8d0b-a93d2e77bd84)

---

### Workflow Overview

1. User selects a command from the Node-RED Dashboard.
2. The ESP receives the command from Node-RED.
3. The ESP appends other details to the command, calculates the CRC of the packet, and then sends it to the STM32 bootloader.
4. The bootloader receives the packet, calculates its CRC to verify that the packet was received successfully.
5. Bootloader performs the operations needed by the command (return data/status to the ESP to be shown in Node-RED)

---

## System Architecture

```
                   Firebase [Update File]
                              |
                              v
[Node-RED Dashboard] <---> [ESP32] <---> [STM32 Custom Bootloader]
```

1. **Firebase** hosts the latest firmware update files.
2. **Node-RED Dashboard** provides a user interface to send commands.
3. **ESP32** acts as a communication bridge between Node-RED and the STM32, and responsible for downloading the update file from firebase storage and transfer it to the STM.
4. **STM32** runs a custom bootloader to manage FOTA updates and execute commands.

---

## Features

- **Upload Application**: Upload new firmware from Firebase to the STM32.
- **Erase Flash**: Erase the STM32 flash memory.
- **Jump to Application**: Jump from the current application to the newer application.
- **Get Chip ID**: Retrieve the STM32's chip ID.
- **Read Protection Level**: Check the current read protection level.

---

## Components

- **STM32F4 Microcontroller** (e.g., STM32F401)
- **ESP32 Module**
- **Node-RED**
- **Firebase**

---

## Setup

1. **Clone the Repository**:

   ```bash
   git clone https://github.com/yourusername/stm32-fota-esp32
   ```

2. **Setup Node-RED**:
   - Install Node-RED: [Node-RED Installation Guide](https://nodered.org/docs/getting-started/)
   - Import the `Node-RED` flow provided in `node-red/flow.json`.
   - ***how to use Node-RED with ESP32: https://youtu.be/N4uf23x4vmM?si=OWFQuKNQUvaNG4Lu***

3. **Flash ESP32**:
   - Flash the `esp32-fota` firmware using PlatformIO or Arduino IDE.

4. **STM32 Bootloader**:
   - Flash the custom bootloader to the STM32 using STM32CubeProgrammer.

4. **STM32 Update file**:
   - Create update file of stm32 using keil IDE.
   - ***See this video: https://youtu.be/0ZRID81G6LU?si=seCyNwVOUL9CGWVb***
   
5. **Firebase Setup**:
   - Host the firmware update files in Firebase.
   - ***See this video: https://youtu.be/MsZfV0x_G8k?si=pQoq8CVdHGI1mdZX***

---

## Node-RED Commands & Components

The following commands can be sent from the Node-RED Dashboard to the STM32 via ESP32:

1. **Upload Application**
2. **Erase Flash**
3. **Jump to Application**
4. **Get Chip ID**
5. **Read Protection Level**
- **Text Field to shown the received data/status from STM32.**
