# EC-M12-BC-C6-C-B Test Procedure

## Overview
The **EC-M12-BC-C6-C-B** is a rugged, IP67-rated IoT telemetry device designed for reliable remote monitoring and industrial data collection. Powered by an ultra-low-power **STM32L072CZT6 MCU** and high-capacity lithium batteries, it supports long-term autonomous operation, cellular connectivity, and RS-485 integration for industrial and environmental applications. 

##Product Used

Product: EC-M12-BC-C6-C-B

More information:
https://norvi.io 

## Purpose of This Example 
This example shows how to: 
- Initialize and configure the EC-M12-BC-C6-C-B controller 
- Read sensor data through the RS-485 Modbus interface 
- Connect to the cellular network using the SIM7600 module 
- Publish sensor values to an MQTT/ThingsBoard server 
- Implement low-power operation using shutdown and wake-up control 

## What the User Should Do

Follow the steps below to run the example.
1.  **Hardware Connections**
- Open the lid of the **EC-M12-BC-C6-C-B** device carefully.
- Change the power supply selection to the **12V side**, since the hydrostatic sensor requires a 12V supply.
- Set the jumper to the **USB programming side**.
- Connect a **Mini USB cable** to the device.
- Carefully connect the **programming header** to the **ST-Link programmer**.
- Move to the **8-pin M8 connector** section.
- Connect the **8-pin M8 cable** to the device.
- Connect the other end of the M8 cable to the **hydrostatic sensor**.
- Place the hydrostatic sensor inside the tank for level measurement.
      
2.  **Program Upload procedure**

   - Follow this guide to program the STM32 board.
   - Open the EC-M12-BC-C6-C-B Functional test program using Arduino IDE 1.8.19.
   - Go to Tools and configure the settings as shown below.

   ![TOOLS](Images/TOOLS.png)

   Generate the binary file for the project:

   **Sketch** **→** **Export Compiled Binary**

   ![BINARY](Images/BINARY.png)

   After compilation is completed, the generated **.bin** file will be available inside the project folder.

   Open the project folder to view the generated binary files: 
   
   **Sketch** **→** **Show Sketch Folder**

   ![SHOW BINARY](Images/SHOW_BINARY.png)

   The sketch folder will open, and the generated **.bin** file can be found inside the folder. 

   - Locate the generated **.bin** file inside the sketch folder and copy its file location/path.
   - Open the STM32 ST-LINK Utility application.

     download the STM32 ST-LINK Utility software from the below link and install it.

     https://www.st.com/en/development-tools/stsw-link004.html

  - Connect the **STM32 board** to the **ST-Link programmer**.

    ![CONNECT](Images/CONNECT.png)

    ![AFTER CONNECTED](Images/AFTER_CONNECTED.png)

    In the STM32 ST-LINK Utility, connect to the target device.

    Go to:
    
      **Target → Program**
    
      ![PROGRAM](Images/PROGRAM.png)
    
      













    

