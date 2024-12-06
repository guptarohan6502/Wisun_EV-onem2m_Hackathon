# Project Documentation

Welcome to the project repository! Below is a detailed explanation of the folder structure and the purpose of each component.

---

## Folder Structure

### 1. **Arduino-App**
   - **Description:** 
     This folder contains the Kotlin-based application designed for Android devices. The app enables communication with the EV charger via BLE (Bluetooth Low Energy).
   - **Purpose:** 
     - Establish BLE connection between the app and the EV charger.
     - Send and receive commands for controlling and monitoring the charger.
   - **Technology Stack:** Kotlin, Android Studio.

---

### 2. **Charger Code**
   This folder is divided into two main subdirectories:

   #### a. **Charger Arduino Code**
   - **Description:** 
     This subdirectory contains the Arduino code that needs to be uploaded to the Nano microcontroller connected to the charger hardware.
   - **Purpose:**
     - Control the physical operations of the charger.
     - Act as an intermediary between the BLE commands from the app and the charger hardware.
   - **Technology Stack:** C/C++ for Arduino.

   #### b. **Main_UI**
   - **Description:**
     This subdirectory contains the codebase for the main user interface (UI) embedded within the EV charger. It handles user interactions and manages communication with other internal components of the charger.
     This code needs to be uploaded on raspberry pi of the charger and just run `python run.py`
   - **Purpose:**
     - Provide a physical user interface for charger controls.
     - Handle core logic for the charger operation, including device control and communication.
   - **Technology Stack:** Pyhon.

---

### 3. **Dashboard Code**
   - **Description:**
     This folder contains the code for the React-based web dashboard that provides real-time updates on the charger and user statistics.
   - **Purpose:**
     - Display user-friendly visualizations and data for charger operations.
     - Provide insights into real-time usage statistics for end-users and administrators.
   - **Technology Stack:** React.js, JavaScript.

---

### 4. **wisun-br-linux-main**
   - **Description:**
     This folder contains the Wi-SUN Linux Border Router project, which implements the [Wi-SUN protocol](https://wi-sun.org/) on Linux devices. It allows Linux hosts to act as Border Routers for Wi-SUN networks.
   - **Purpose:**
     - Run the `wsbrd` daemon to manage the high-level layers of the Wi-SUN protocol.
     - Enable communication with RCP (Radio Co-Processor) devices over serial connections (e.g., UART).
   - **Quick Start:**
     - The project requires dependencies like `mbedTLS`, `libnl-3`, and `libnl-route-3` for compilation.
     - Refer to the folder’s dedicated [README.md](wisun-br-linux-main/README.md) for detailed instructions on setup, compilation, and usage.
   - **Technology Stack:** C, Linux, mbedTLS, libnl.

---

## How to Get Started
1. **Arduino-App:**
   - Open the project in Android Studio.
   - Build and deploy the app to an Android device.
   - Use the app to establish a BLE connection with the charger.

2. **Charger Code:**
   - Navigate to `Charger Arduino Code` to upload the code to the Arduino Nano.
   - Use the `Main_UI` directory to manage the user interface and program logic within the EV charger.

3. **Dashboard Code:**
   - Navigate to the `Dashboard Code` folder.
   - Install dependencies using `npm install`.
   - Run the dashboard locally using `npm start`.

4. **wisun-br-linux-main:**
   - Follow the instructions in the folder’s `README.md` to set up and run the Wi-SUN Border Router.

---

## Contribution Guidelines
- Follow the established coding conventions for each component.
- Submit pull requests with clear and detailed descriptions of changes.
- Test your code thoroughly before submission.
  
---

## Contact
For further details or issues, please contact the project maintainer at rohangupta6502@gmail.com
