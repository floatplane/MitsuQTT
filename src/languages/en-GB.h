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

// Page Control
const char txt_ctrl_title[] PROGMEM = "Control Unit";
const char txt_ctrl_temp[] PROGMEM = "Temperature";
const char txt_ctrl_power[] PROGMEM = "Power";
const char txt_ctrl_mode[] PROGMEM = "Mode";
const char txt_ctrl_fan[] PROGMEM = "Fan";
const char txt_ctrl_vane[] PROGMEM = "Vane";
const char txt_ctrl_wvane[] PROGMEM = "Wide Vane";
const char txt_ctrl_ctemp[] PROGMEM = "Current temperature";

// Page Login
const char txt_login_title[] PROGMEM = "Authentication";
const char txt_login_password[] PROGMEM = "Password";
const char txt_login_sucess[] PROGMEM =
    "Login successful, you will be redirected in a few seconds.";
const char txt_login_fail[] PROGMEM = "Wrong username/password! Try again.";
