# Overview
The `lidar` service interfaces with an **SLAMTEC RPLIDAR A2M8** sensor to capture and profcess 2D laser scan data.

## How it works

### 1. Initialization & Configuration:
The service reads configuration values for the speed and the mode. The speed can takes rpm values in the range of 300-900 with a default value of 600. If an incorrect value is inputed, it is handled by the driver implementation and it should just use the default one. The mode can be **0 (for Standard)**, **1 (for Express)** and **2 (for Boost)**.

Then it creates a serial connection (`/dev/ttyUSB0`) with a baudrate of **115200**, from where it retrieves the device information and prints it.

### 2. Data Acquisition & Processing:
The device can take up to 1024 measurement nodes per scan. The raw data is sorted then in ascending order by angle, then it converts the angles in degrees and the distance in millimeters. In the end the angle, distance, quality and start flag are sent in an array in a protobuf message with a timestamp.

Below is an example plot of the lidar scan data:

![Example Lidar Plot](https://github.com/user-attachments/assets/b073d3a1-81de-4da2-ac49-d215327075f0)


### 3. Graceful Shutdown:
**SIGINT** and **SIGTERM** signals are caught to stop the LIDAR motor and clean up all the memory allocated to the scans arrays. This ensures a safe shutdown of the service and the LIDAR sensor.

## Reference
- For more detailed specifications and additional information on the **SLAMTEC RPLIDAR A2M8** sensor, please refer to the official documentation: [RPLIDAR A Series Support](https://www.slamtec.com/en/Support#rplidar-a-series).
- For the source code and further details about the SDK, check out the GitHub repository: [slamtec/rplidar_sdk](https://github.com/slamtec/rplidar_sdk).
