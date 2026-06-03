# Quick Start Guide

## 5-Minute Setup

### Step 1: Prerequisites Check
```bash
# Verify ESP-IDF is installed
idf.py version

# Should output: ESP-IDF v5.2.x or higher
```

### Step 2: Configure Your Hardware

Edit `main/config.h`:

```c
// WiFi
#define WIFI_SSID               "your_home_wifi"
#define WIFI_PASSWORD           "your_wifi_password"

// MQTT Broker
#define MQTT_BROKER_URI         "mqtt://192.168.1.100:1883"
#define MQTT_USERNAME           "weather_user"
#define MQTT_PASSWORD           "mqtt_password"

// RS485 GPIO Pins (adjust if different)
#define RS485_UART_TXD          GPIO_NUM_17
#define RS485_UART_RXD          GPIO_NUM_16
#define RS485_UART_RTS          GPIO_NUM_18
```

### Step 3: Build & Flash

```bash
# Set target to ESP32-S3
idf.py set-target esp32s3

# Build the firmware
idf.py build

# Flash to your device (replace /dev/ttyUSB0 with your port)
idf.py -p /dev/ttyUSB0 flash

# Monitor the output
idf.py -p /dev/ttyUSB0 monitor
```

### Step 4: Verify Operation

You should see in the logs:
```
[MAIN] Weather Station Firmware Started
[MAIN] Initializing system components...
[MAIN] Connecting to WiFi...
[MQTT] MQTT client initialized
[MAIN] Waiting for sensor data...
```

## Testing with MQTT

### Install MQTT Client Tool
```bash
# Ubuntu/Debian
sudo apt-get install mosquitto-clients

# macOS
brew install mosquitto
```

### Subscribe to Topics
```bash
# Open a terminal and subscribe to all weather topics
mosquitto_sub -h your_mqtt_broker -u weather_user -P mqtt_password -t 'weather/#'

# You should see messages like:
# weather/temperature 25.50
# weather/humidity 65.30
# weather/wind_speed 3.45
```

### Publish Test Commands
```bash
# Test reset command
mosquitto_pub -h your_mqtt_broker -u weather_user -P mqtt_password \
  -t 'weather/command' -m 'reset'

# Test time sync
mosquitto_pub -h your_mqtt_broker -u weather_user -P mqtt_password \
  -t 'weather/command' -m 'sync_time'
```

## Common Commands

```bash
# View all logs
idf.py -p /dev/ttyUSB0 monitor

# Build specific component
idf.py build main

# Full rebuild
idf.py clean && idf.py build

# Erase flash memory
idf.py -p /dev/ttyUSB0 erase-flash

# Check connected devices
ls /dev/ttyUSB*  # Linux
ls /dev/tty.usbserial*  # macOS
```

## Troubleshooting

### Issue: "Failed to connect to board"
```bash
# Check USB port
ls /dev/ttyUSB*

# Grant permissions (Linux)
sudo usermod -a -G dialout $USER
sudo chmod 666 /dev/ttyUSB0

# Reconnect device
```

### Issue: "No module named 'yaml'"
```bash
# Install required Python modules
pip install pyyaml
pip install ecdsa
pip install bitstring
```

### Issue: No data from weather station
1. Check RS485 wiring with multimeter
2. Verify baud rate is 9600
3. Test with RS485 analyzer tool
4. Check sensor is powered on
5. Look for frame timeout errors in logs

## Next Steps

1. **Customize Data Logging**: Edit `LOG_INTERVAL_SECONDS` in config.h
2. **Add Data Validation**: Modify `parser_validate_data()` in sensor_parser.c
3. **Implement Cloud Integration**: Add cloud API calls in main.c
4. **Battery Optimization**: Reduce publish frequency for battery-powered operation
5. **Web Dashboard**: Create a dashboard subscribing to MQTT topics

## File Modifications Checklist

- [ ] config.h - WiFi and MQTT credentials
- [ ] main/config.h - GPIO pin assignments
- [ ] partitions.csv - If using different flash size
- [ ] sdkconfig - For custom build options

## Support

For issues or questions:
1. Check the logs: `idf.py monitor`
2. Review config.h settings
3. Test MQTT connectivity separately
4. Verify hardware connections

---

**Ready?** Start with: `idf.py set-target esp32s3 && idf.py build`
