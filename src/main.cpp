// Need to use extern "C" to prevent C++ name mangling
extern "C" {
  #include "../lib/include/roverlib.h" 
}
#include <iostream>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include "sl_lidar.h" 
#include "sl_lidar_driver.h"

using namespace sl;

ILidarDriver * drv;


long long current_time_millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}


// The main user space program
// this program has all you need from roverlib: service identity, reading, writing and configuration
int user_program(Service service, Service_configuration *configuration) {
  // Set stdout buffer to NULL for immediate output
  setbuf(stdout, NULL);

  // Check if configuration is loaded correctly
  if (configuration == NULL) {
    printf("Configuration cannot be accessed\n");
    return 1;
  }

  // Get the configuration values for speed and mode
  double *speed = get_float_value_safe(configuration, (char*)"speed");
  if (speed == NULL) {
    printf("Failed to get configuration\n");
    return 1;
  }
  printf("Fetched runtime configuration example tunable number: %f\n", *speed);

  double *mode = get_float_value_safe(configuration, (char*)"mode");
  if (mode == NULL) {
    printf("Failed to get configuration\n");
    return 1;
  }
  printf("Fetched runtime configuration example tunable number: %f\n", *mode);

  // Create a write stream to send the lidar data
  write_stream *write_stream = get_write_stream(&service, (char*)"lidar-data");
  if (write_stream == NULL) {
    printf("Failed to create write stream 'decision'\n");
  }
  
  // Create a serial port channel
  // The serial port is set to /dev/ttyUSB0 and the baudrate is set to 115200 
  IChannel* _channel;
  _channel = (*createSerialPortChannel("/dev/ttyUSB0", 115200));
  // Create a LIDAR driver
  drv = *createLidarDriver();

  // Use driver to connect to the LIDAR via the serial port channel
  auto res = drv->connect(_channel);
  // Create a device info object to hold the device information
  sl_lidar_response_device_info_t deviceInfo;
  // Check if the connection was successful and print the device information
  if(SL_IS_OK(res)) {
    res = drv->getDeviceInfo(deviceInfo);
    if(SL_IS_OK(res)){
      printf("Model: %d, Firmware Version: %d.%d, Hardware Version: %d\n",
      deviceInfo.model,
      deviceInfo.firmware_version >> 8, deviceInfo.firmware_version & 0xffu,
      deviceInfo.hardware_version);
    }
    else {
      fprintf(stderr, "Failed to get device information from LIDAR %08x\r\n", res);
    }
  }
  else {
    fprintf(stderr, "Failed to connect to LIDAR %08x\r\n", res);
  }

  // Create vector to hold the possible scan modes
  std::vector<LidarScanMode> scanModes;
  drv->getAllSupportedScanModes(scanModes);

  // sets the speed of the motor from the configuration value
  drv->setMotorSpeed(static_cast<sl_u16> (*speed));
  
  // Check if the mode from the configuration is valid and set the scan mode
  if (*mode == 0 || *mode == 1 || *mode == 2) {
    drv->startScanExpress(false, scanModes[static_cast<size_t> (*mode)].id);
  } 
  else {
    printf("Invalid mode value, please use either 0 (Standard), 1 (Express) or 2 (Boost)\n");
    drv->stop();
    drv->setMotorSpeed(0);
    delete drv;
    delete _channel;
    return 1;
  }

  while (true) {
    // Allocate an array to hold up to 1024 high-quality LIDAR measurement nodes
    // These nodes will store the raw scan data from the LIDAR sensor
    sl_lidar_response_measurement_node_hq_t nodes[1024];

    // Calculate the total number of nodes available in the array.
    size_t nodeCount = sizeof(nodes)/sizeof(sl_lidar_response_measurement_node_hq_t);

    // Request high-quality scan data from the LIDAR driver and store it in the nodes array
    // The 'nodeCount' indicates the maximum number of measurements to be grabbed
    res = drv->grabScanDataHq(nodes, nodeCount);

    if(SL_IS_OK(res)) {
      // Sort the scan data in ascending order based on the angle
      drv->ascendScanData(nodes, nodeCount);

      // Create a protobuf message to send
      ProtobufMsgs__SensorOutput lidar_msg = PROTOBUF_MSGS__SENSOR_OUTPUT__INIT;
      lidar_msg.timestamp = current_time_millis();
      lidar_msg.status = 0;
      lidar_msg.sensorid = 1;
      // Set the oneof field contents
      ProtobufMsgs__LidarSensorOutput lidar_sensor_output = PROTOBUF_MSGS__LIDAR_SENSOR_OUTPUT__INIT;
      lidar_sensor_output.n_scans = nodeCount;
      // Allocate memory for the scans array
      lidar_sensor_output.scans = (ProtobufMsgs__LidarSensorOutput__Scan**)malloc(sizeof(ProtobufMsgs__LidarSensorOutput__Scan) * nodeCount);
      for (int i = 0; i < nodeCount; i++) {
        // Allocate memory for each scan message
        lidar_sensor_output.scans[i] = (ProtobufMsgs__LidarSensorOutput__Scan*)malloc(sizeof(ProtobufMsgs__LidarSensorOutput__Scan));
        // Check if memory allocation was successful
        if (lidar_sensor_output.scans[i] == NULL) {
          fprintf(stderr, "Error: Failed to allocate memory for lidar scan at index %d\n", i);
          for (int j = 0; j < i; j++) {
            free(lidar_sensor_output.scans[j]);
          }
          free(lidar_sensor_output.scans);
          return 1;
        }
        // Initialize the scan message structure
        *lidar_sensor_output.scans[i] = PROTOBUF_MSGS__LIDAR_SENSOR_OUTPUT__SCAN__INIT;
        // Store angle data in degrees
        lidar_sensor_output.scans[i]->angle = (nodes[i].angle_z_q14 * 90.f) / 16384.f;
        // Store distance data in millimeters
        lidar_sensor_output.scans[i]->distance = nodes[i].dist_mm_q2/4.0f;
        // Store the quality of the measurement
        lidar_sensor_output.scans[i]->quality = nodes[i].quality >> SL_LIDAR_RESP_MEASUREMENT_QUALITY_SHIFT;
        // Store the start flag
        // This flag indicates if the measurement is the start of a new scan (1) or not (0)
        lidar_sensor_output.scans[i]->isstart = (nodes[i].flag & SL_LIDAR_RESP_HQ_FLAG_SYNCBIT) ? 1 : 0;
      }
      
      // Set the oneof field (union)
      lidar_msg.lidaroutput = &lidar_sensor_output;
      lidar_msg.sensor_output_case = PROTOBUF_MSGS__SENSOR_OUTPUT__SENSOR_OUTPUT_LIDAR_OUTPUT;

      // Send the message
      int res = write_pb(write_stream, &lidar_msg);
      if (res <= 0) {
        printf("Could not write 'lidar-data' message\n");
        return 1;
      }

      // Free the memory allocated for the lidar sensor output scans
      for (int i = 0; i < nodeCount; i++) {
        free(lidar_sensor_output.scans[i]);
      }
      free(lidar_sensor_output.scans);
    }
  }
  // Stop the LIDAR motor and clean up
  drv->stop();
  drv->setMotorSpeed(0);
  delete drv;
  delete _channel;
  return 0;
}

int on_terminate(int signum) {
  // This function is called when the service is terminated
  // You can do any cleanup here, like closing files, sockets, etc.
  printf("Service terminated with signal %d, gracefully shutting down\n", signum);
  printf("stopping lidar motor\n",);
  // Stop the LIDAR motor and clean up
  drv->stop();
  drv->setMotorSpeed(0);
  delete drv;
  delete _channel;

  fflush(stdout); // Ensure all output is printed before exit
  return 0; // Return 0 to indicate successful termination
}

// This is just a wrapper to run the user program
// it is not recommended to put any other logic here
int main() {
  return run(user_program, on_terminate);
}