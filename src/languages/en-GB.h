/*
  mitsubishi2mqtt - Mitsubishi Heat Pump to MQTT control for Home Assistant.
  Copyright (c) 2022 gysmo38, dzungpv, shampeon, endeavour, jascdk, chrdavis,
  alekslyse.  All right reserved. This library is free software; you can
  redistribute it and/or modify it under the terms of the GNU Lesser General
  Public License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once
#include <Arduino.h>

// Main Menu
const char txt_control[] PROGMEM = "Control";
const char txt_setup[] PROGMEM = "Setup";
const char txt_status[] PROGMEM = "Status";
const char txt_firmware_upgrade[] PROGMEM = "Firmware Upgrade";
const char txt_reboot[] PROGMEM = "Reboot";

// Setup Menu
const char txt_MQTT[] PROGMEM = "MQTT";
const char txt_WIFI[] PROGMEM = "WIFI";
const char txt_unit[] PROGMEM = "Unit";
const char txt_others[] PROGMEM = "Others";
const char txt_reset[] PROGMEM = "Reset configuration";
const char txt_reset_confirm[] PROGMEM = "Do you really want to reset this unit?";

// Buttons
const char txt_back[] PROGMEM = "Back";
const char txt_save[] PROGMEM = "Save & Reboot";
const char txt_logout[] PROGMEM = "Logout";
const char txt_login[] PROGMEM = "LOGIN";

// Form choices
const char txt_f_on[] PROGMEM = "ON";
const char txt_f_off[] PROGMEM = "OFF";
const char txt_f_auto[] PROGMEM = "AUTO";
const char txt_f_heat[] PROGMEM = "HEAT";
const char txt_f_dry[] PROGMEM = "DRY";
const char txt_f_cool[] PROGMEM = "COOL";
const char txt_f_fan[] PROGMEM = "FAN";
const char txt_f_quiet[] PROGMEM = "QUIET";
const char txt_f_speed[] PROGMEM = "SPEED";
const char txt_f_swing[] PROGMEM = "SWING";
const char txt_f_pos[] PROGMEM = "POSITION";
const char txt_f_celsius[] PROGMEM = "Celsius";
const char txt_f_fh[] PROGMEM = "Fahrenheit";
const char txt_f_allmodes[] PROGMEM = "All modes";
const char txt_f_noheat[] PROGMEM = "All modes except heat";

// Page WIFI
const char txt_wifi_title[] PROGMEM = "WIFI Parameters";
const char txt_wifi_hostname[] PROGMEM = "Hostname";
const char txt_wifi_SSID[] PROGMEM = "SSID";
const char txt_wifi_psk[] PROGMEM = "PSK";
const char txt_wifi_otap[] PROGMEM = "OTA Password";

// Page Control
const char txt_ctrl_title[] PROGMEM = "Control Unit";
const char txt_ctrl_temp[] PROGMEM = "Temperature";
const char txt_ctrl_power[] PROGMEM = "Power";
const char txt_ctrl_mode[] PROGMEM = "Mode";
const char txt_ctrl_fan[] PROGMEM = "Fan";
const char txt_ctrl_vane[] PROGMEM = "Vane";
const char txt_ctrl_wvane[] PROGMEM = "Wide Vane";
const char txt_ctrl_ctemp[] PROGMEM = "Current temperature";

// Page Unit
const char txt_unit_title[] PROGMEM = "Unit configuration";
const char txt_unit_temp[] PROGMEM = "Temperature unit";
const char txt_unit_maxtemp[] PROGMEM = "Maximum temperature";
const char txt_unit_mintemp[] PROGMEM = "Minimum temperature";
const char txt_unit_steptemp[] PROGMEM = "Temperature step";
const char txt_unit_modes[] PROGMEM = "Mode support";
const char txt_unit_password[] PROGMEM = "Web password";

// Page Login
const char txt_login_title[] PROGMEM = "Authentication";
const char txt_login_password[] PROGMEM = "Password";
const char txt_login_sucess[] PROGMEM =
    "Login successful, you will be redirected in a few seconds.";
const char txt_login_fail[] PROGMEM = "Wrong username/password! Try again.";

// Page Init
const char txt_init_title[] PROGMEM = "Initial setup";
const char txt_init_reboot_mes[] PROGMEM =
    "Rebooting and connecting to your WiFi network! You should see it listed "
    "in on your access point.";
const char txt_init_reboot[] PROGMEM = "Rebooting...";
