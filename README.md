# Home Sensor

An ESP32-based home sensor that measures temperature and humidity using a DHT22 and displays the readings on two OLED screens.

![ESP32 home sensor prototype displaying temperature and humidity](images/Prototype_vol1.jpeg)

The firmware reads the sensor every three seconds and calculates the dew point. The second screen alternates between humidity and dew point, and the minimum and maximum temperatures recorded since startup.

## Two-screen prototype

| Humidity and dew point | Temperature history |
| :---: | :---: |
| ![The prototype showing humidity and dew point](images/air-conditions.jpg) | ![The prototype showing minimum and maximum temperatures](images/temperature-history.jpg) |

This is an early prototype and a foundation for future additions, including more sensors and data logging.
