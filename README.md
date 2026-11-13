# Home Sensor

An ESP32-based home sensor that monitors temperature and humidity with a DHT22 and displays the readings on two OLED screens.

| Humidity and dew point | Temperature history |
| :---: | :---: |
| ![The prototype showing humidity and dew point](images/air-conditions.jpg) | ![The prototype showing minimum and maximum temperatures](images/temperature-history.jpg) |

## How it works

The firmware reads temperature and relative humidity every three seconds. It uses these values to estimate the **dew point** with the Magnus formula. The dew point is the temperature at which water vapour in the air can begin to condense on a sufficiently cold surface.

- **First display:** current temperature inside an animated circle.
- **Second display:** alternates every 15 seconds between humidity and dew point, and minimum and maximum temperature.

The minimum and maximum are recorded since the ESP32 was last started and reset when it restarts. Invalid sensor readings are reported and are not used to update these values. Readings are also sent to the serial monitor.

This is an early prototype and a foundation for future additions, including more sensors and data logging.

## Local Wi-Fi dashboard

The ESP32 hosts a web page on the local network with temperature, humidity and dew point. The page refreshes every three seconds. It does not store a history. Both the ESP32 and the phone or computer need to be able to reach each other on the same local network; guest network isolation can prevent this.

1. Copy `Home_sensor/wifi_secrets.example.h` to `Home_sensor/wifi_secrets.h` and enter your Wi-Fi name and password. The ESP32 connects to 2.4 GHz Wi-Fi. The secrets file is ignored by Git; never commit your credentials.
2. Upload the sketch to the ESP32 DevKit and open Serial Monitor at 115200 baud.
3. When connected, the ESP32 prints a local address such as `http://192.168.1.42`. Open that address in a browser on your phone or computer. The address may change after a restart.

The displays and sensor keep working while Wi-Fi is unavailable, and the ESP32 attempts to reconnect. The dashboard has no login and is intended for a trusted local network; do not expose it directly to the internet.
