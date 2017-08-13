# arduino-mqtt-socket

Control an mains socket with an ESP8266 (in my case, a Wemos D1 mini clone) connected to a mosquitto broker
Sends 'up' when connected,
Connects with a LastWillMessage of 'down'
Switches the relay ON on a 1
Switches the relay OFF on a 0
Toggles the relay on a 2

TODO:
- [x] Add a page to setup what can be setup
- [x] Store broker data into EEPROM
- [x] Add a WiFi portal to set up the connection if the wifi is unreachable
- [ ] Betterify
