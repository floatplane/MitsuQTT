/*
  MitsuQTT Copyright (c) 2024 floatplane

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU
  Lesser General Public License as published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version. This library is distributed in the hope that
  it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License along with this library;
  if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA
*/

#include "templates.hpp"

#define INCBIN_PREFIX
#include "incbin.h"

#define STRINGIFY_1(s) #s
#define STRINGIFY(s) STRINGIFY_1(s)

INCTXT(countdown, "src/frontend/" STRINGIFY(LANGUAGE) "/partials/countdown.mst");
INCTXT(footer, "src/frontend/" STRINGIFY(LANGUAGE) "/partials/footer.mst");
INCTXT(header, "src/frontend/" STRINGIFY(LANGUAGE) "/partials/header.mst");

INCTXT(autoconfig, "src/frontend/" STRINGIFY(LANGUAGE) "/views/autoconfig.mst");
INCTXT(captiveIndex, "src/frontend/" STRINGIFY(LANGUAGE) "/views/captive/index.mst");
INCTXT(captiveReboot, "src/frontend/" STRINGIFY(LANGUAGE) "/views/captive/reboot.mst");
INCTXT(captiveSave, "src/frontend/" STRINGIFY(LANGUAGE) "/views/captive/save.mst");
INCTXT(control, "src/frontend/" STRINGIFY(LANGUAGE) "/views/control.mst");
INCTXT(index, "src/frontend/" STRINGIFY(LANGUAGE) "/views/index.mst");
INCTXT(login, "src/frontend/" STRINGIFY(LANGUAGE) "/views/login.mst");
INCTXT(metrics, "src/frontend/" STRINGIFY(LANGUAGE) "/views/metrics.mst");
INCTXT(mqttIndex, "src/frontend/" STRINGIFY(LANGUAGE) "/views/mqtt/index.mst");
INCTXT(mqttTextField, "src/frontend/" STRINGIFY(LANGUAGE) "/views/mqtt/_text_field.mst");
INCTXT(others, "src/frontend/" STRINGIFY(LANGUAGE) "/views/others.mst");
INCTXT(reboot, "src/frontend/" STRINGIFY(LANGUAGE) "/views/reboot.mst");
INCTXT(reset, "src/frontend/" STRINGIFY(LANGUAGE) "/views/reset.mst");
INCTXT(setup, "src/frontend/" STRINGIFY(LANGUAGE) "/views/setup.mst");
INCTXT(status, "src/frontend/" STRINGIFY(LANGUAGE) "/views/status.mst");
INCTXT(unit, "src/frontend/" STRINGIFY(LANGUAGE) "/views/unit.mst");
INCTXT(upgrade, "src/frontend/" STRINGIFY(LANGUAGE) "/views/upgrade.mst");
INCTXT(upload, "src/frontend/" STRINGIFY(LANGUAGE) "/views/upload.mst");
INCTXT(wifi, "src/frontend/" STRINGIFY(LANGUAGE) "/views/wifi.mst");

INCTXT(css, "src/frontend/statics/mvp.css");

namespace partials {
const __FlashStringHelper *countdown = FPSTR(countdownData);
const __FlashStringHelper *footer = FPSTR(footerData);
const __FlashStringHelper *header = FPSTR(headerData);
};  // namespace partials

namespace views {
const __FlashStringHelper *autoconfig = FPSTR(autoconfigData);

namespace captive {
const __FlashStringHelper *index = FPSTR(captiveIndexData);
const __FlashStringHelper *reboot = FPSTR(captiveRebootData);
const __FlashStringHelper *save = FPSTR(captiveSaveData);
}  // namespace captive

const __FlashStringHelper *control = FPSTR(controlData);
const __FlashStringHelper *index = FPSTR(indexData);
const __FlashStringHelper *login = FPSTR(loginData);
const __FlashStringHelper *metrics = FPSTR(metricsData);

namespace mqtt {
const __FlashStringHelper *index = FPSTR(mqttIndexData);
const __FlashStringHelper *textField = FPSTR(mqttTextFieldData);
}  // namespace mqtt

const __FlashStringHelper *others = FPSTR(othersData);
const __FlashStringHelper *reboot = FPSTR(rebootData);
const __FlashStringHelper *reset = FPSTR(resetData);
const __FlashStringHelper *setup = FPSTR(setupData);
const __FlashStringHelper *status = FPSTR(statusData);
const __FlashStringHelper *unit = FPSTR(unitData);
const __FlashStringHelper *upgrade = FPSTR(upgradeData);
const __FlashStringHelper *upload = FPSTR(uploadData);
const __FlashStringHelper *wifi = FPSTR(wifiData);
};  // namespace views

namespace statics {
const __FlashStringHelper *css = FPSTR(cssData);
}