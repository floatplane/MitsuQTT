# MitsuQTT

_pronounced Mitsu-cute_

[![.github/workflows/build.yml](https://github.com/floatplane/MitsuQTT/actions/workflows/build.yml/badge.svg)](https://github.com/floatplane/MitsuQTT/actions/workflows/build.yml)
[![.github/workflows/test.yml](https://github.com/floatplane/MitsuQTT/actions/workflows/test.yml/badge.svg)](https://github.com/floatplane/MitsuQTT/actions/workflows/test.yml)
[![.github/workflows/static_analysis.yml](https://github.com/floatplane/MitsuQTT/actions/workflows/static_analysis.yml/badge.svg)](https://github.com/floatplane/MitsuQTT/actions/workflows/static_analysis.yml)
[![.github/workflows/clangformat.yml](https://github.com/floatplane/MitsuQTT/actions/workflows/clangformat.yml/badge.svg)](https://github.com/floatplane/MitsuQTT/actions/workflows/clangformat.yml)
[![BuyMeACoffee](https://raw.githubusercontent.com/pachadotdev/buymeacoffee-badges/main/bmc-donate-yellow.svg)](https://www.buymeacoffee.com/floatplane)

MitsuQTT is an embedded application that runs on ESP8266/ESP32 hardware and provides the following functionality:
- Control of an attached Mitsubishi heat pump via the heat pump's CN105 connector
- An MQTT interface that can both publish the current heat pump state *and* accept commands to change it
- Home Assistant autodiscovery - the application can show up in HA as a climate entity
- An embedded webserver for configuration and communication

MitsuQTT is a drop-in replacement for the [mitsubishi2MQTT](https://github.com/gysmo38/mitsubishi2MQTT) project, with some notable improvements:
- Automatic dark/light mode support in the UI
- Web performance greatly improved
- More rigorous testing - unit tests, automated testing via clang-tidy and cppcheck
- [Safe mode](#safe-mode), for air handlers that rely on external temperature sensors to work correctly
- Additional [monitoring](#monitoring) support

## Screenshots
homepage | control | status page | light mode
--- | --- | --- | ---
![home page](https://github.com/floatplane/MitsuQTT/assets/101196/6f26babe-1078-4f67-a8a4-2d26e6ebaf30) | ![control](https://github.com/floatplane/MitsuQTT/assets/101196/7d6da0a5-cb74-4a5a-9459-7cea74c0fbfb) | ![status](https://github.com/floatplane/MitsuQTT/assets/101196/da2afef2-066d-4d65-9e60-aff2b30618a1) | ![light mode](https://github.com/floatplane/MitsuQTT/assets/101196/21ff5506-c984-44a6-8af1-26bc70a34ccd)


## Setup

### Hardware
You're going to need to get some hardware connected to your heat pump. Here are some links I found helpful:
- https://www.instructables.com/Wemos-ESP8266-Getting-Started-Guide-Wemos-101/
- https://community.home-assistant.io/t/mitsubishi-ac-mqtt-temperature/269979/5
- https://chrdavis.github.io/hacking-a-mitsubishi-heat-pump-Part-2/
    (this page references mitsubishi2MQTT, but MitsuQTT can be used as a drop-in replacement)


### Initial software setup
This project uses [PlatformIO](https://platformio.org/) to build. Recommmended: install the PlatformIO IDE through VS Code. From VS Code, it should be one button press to build and deploy to your selected hardware:

https://github.com/floatplane/MitsuQTT/assets/101196/1e14b3e7-e1f5-4804-876f-b56db5232b45

You should only have to build and deploy the code once onto your hardware - subsequent updates can be done over-the-air (OTA).

### Configuration
If all goes well, the WiFi hardware will create its own ad-hoc network that you can join. Look for an access point of the form `HVAC_XXXX` and connect to it. Then visit http://192.168.1.1 or http://setup to get to the initial configuration screen. Here, you'll enter a name for your hardware (use a hostname - no spaces!) as well as login information for the wireless network you want the hardware to connect to.

After saving and restarting, you'll be ready to configure your hardware through the Setup screen - in particular, you'll want to set up an MQTT connection to use this with Home Assistant, Node-RED, or any other automation technology.

### OTA updates
You can download the latest versions [here](https://nightly.link/floatplane/MitsuQTT/workflows/build/main?preview). Pick the download for your hardware type, unzip it on your desktop, and use the "Firmware Update" page to upload the BIN file to your hardware.

#### Should I use a SPIFFS or LittleFS version?
There are two incompatible filesystems for ESP8266/ESP32 - LittleFS and SPIFFS. LittleFS is the more modern filesystem and should be used by default. SPIFFS is supported for backwards compatibility when upgrading from older versions, but support will be removed at some point. If you know you need SPIFFS, use a firmware version with SPIFFS in the name. Don't worry, the upload page will try to warn you if you've picked incorrectly!

### Testing web changes locally
There's a simple webserver you can launch by running `scripts/webserver.rb`. It will render page templates with a fixed set of data, so it's not a true simulator - but it's helpful when iterating on frontend changes.

---
## Monitoring
MitsuQTT exposes a `/metrics.json` endpoint that can be used to directly interrogate the state of the hardware. You can connect this to a tool like [Uptime Kuma](https://github.com/louislam/uptime-kuma) to watch for changes and publish alerts:

<img width="1288" alt="image" src="https://github.com/floatplane/MitsuQTT/assets/101196/37652cf3-f7f9-4a14-ba15-4a69f2a17cef">

## Safe mode
Safe mode is for **air handlers**: units that are designed to be replacements for legacy furnaces. Air handlers typically rely on getting a current temperature reading from a remote thermostat, since the ambient temperature they read in a basement can be wildly different from the temperature in the living space. If a connection failure prevents MitsuQTT from receiving remote temperature updates, the default behavior is to revert to the internal temperature sensor - fine for wall-mounted indoor units, but disastrous for air handlers that believe that the room temperature has dropped by 10 degrees, and start heating to compensate.

MitsuQTT offers a "safe mode" option, which will shut the air handler down when remote temperature signals are not being received, and turn it back on when they resume. Safe mode activation can be observed through `/metrics.json`, and thus turned into an actionable alert:
```json5
$ curl http://main_floor_heatpump/metrics.json
{
  "hostname": "main_floor_heatpump",
  "version": "2024.03.17",
  "git_hash": "3daa0ce",
  "status": {
    // If `true`, then safe mode has been activated and the heat pump
    // will ignore all MQTT requests to turn on
    "safeModeLockout": false
  },
  // ...
}
```


## Node-RED control
You can post messages to MQTT to control the heat pump:

- topic/power/set OFF
- topic/mode/set AUTO HEAT COOL DRY FAN_ONLY OFF ON
- topic/temp/set 16-31
- topic/remote_temp/set also called "room_temp", the implementation defined in "HeatPump" seems not work in some models
- topic/fan/set 1-4 AUTO QUIET
- topic/vane/set 1-5 SWING AUTO
- topic/wideVane/set << < | > >>
- topic/settings
- topic/state
- topic/debug/packets
- topic/debug/packets/set on off
- topic/debug/logs
- topic/debug/logs/set on off
- topic/custom/send as example "fc 42 01 30 10 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 7b " see https://github.com/SwiCago/HeatPump/blob/master/src/HeatPump.h
- topic/system/set reboot 

## Grafana dashboard

_note: this was copied from Mitsubishi2MQTT, but is not well tested. file an issue if you have problems!_

To use Grafana you need to have Prometheus and Grafana (v10 or newer) installed.
Config for Prometheus:
```  - job_name: MitsuQTT
    static_configs:
        - targets:
            - IP-TO-MitsuQTT
```
Then add Prometheus as a datasource in Grafana
Grafana -> Connections -> Add new connection -> Prometheus -> ```Prometheus server URL: PROMETHEUS-IP:PORT```

Then you can import the dashboard in Grafana -> Dashboards -> New -> Import and upload the file https://github.com/floatplane/MitsuQTT/blob/master/misc/prometheus.json

## Credits

MitsuQTT started as a fork of [gysmo38/mitsubishi2MQTT](https://github.com/gysmo38/mitsubishi2MQTT), though it's evolved quite a bit since then.

Heat pump control is via [SwiCago/HeatPump](https://github.com/SwiCago/HeatPump).

## Support 

I hope this is useful to you! If it is, then maybe you'd like to buy me a coffee? :blush:

<a href="https://www.buymeacoffee.com/floatplane" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-violet.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>
