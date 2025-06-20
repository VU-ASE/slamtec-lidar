# Usage
`lidar` does not take any input and continuously outputs a `lidar-data` stream. It contains an array of scans which are sorted in ascending order based on the angle value.

## Output
`lidar` outputs the `lidar-data` stream as seen in the example below
```
pb_output.SensorOutput{
    SensorId:  1,
    Timestamp: uint64(time.Now().UnixMilli()),
    SensorOutput: &pb_output.SensorOutput_LidarOutput{
        LidarOutput: &pb_output.LidarSensorOutput{
            NScans: <nodeCount>,
            Scans: []*pb_output.LidarSensorOutput_Scan{
                {
                    Angle:    float32(angle in degrees),
                    Distance: float32(distance in millimeters),
                    Quality:  uint32(measurement quality),
                    IsStart:  bool (true if scan start, false otherwise),
                },
                // ... additional scan points
            },
        },
    },
}
```

`SensorOutput_LidarOutput` contains a `LidarOutput` object, which contains the total number of scans (`NScans`) and an array of `Scan` objects.

Each `Scan` includes:
1. `Angle`, a `float32` value representing the angle in degrees.
2. `Distance`, a `float32` value representing the distance in millimeters.
3. `Quality`, an `uint32` value indicating the measurement quality.
4. `IsStart`, a `boolean` value indicating whether this measurement marks the start of a new scan.
