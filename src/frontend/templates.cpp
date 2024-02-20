#include "templates.hpp"

#define INCBIN_PREFIX
#include "incbin.h"

#define STRINGIFY_1(s) #s
#define STRINGIFY(s) STRINGIFY_1(s)

INCTXT(footer, "src/frontend/" STRINGIFY(LANGUAGE) "/partials/footer.mst");
INCTXT(header, "src/frontend/" STRINGIFY(LANGUAGE) "/partials/header.mst");

INCTXT(autoconfig, "src/frontend/" STRINGIFY(LANGUAGE) "/views/autoconfig.mst");
INCTXT(captiveIndex, "src/frontend/" STRINGIFY(LANGUAGE) "/views/captive/index.mst");
INCTXT(captiveReboot, "src/frontend/" STRINGIFY(LANGUAGE) "/views/captive/reboot.mst");
INCTXT(captiveSave, "src/frontend/" STRINGIFY(LANGUAGE) "/views/captive/save.mst");
INCTXT(metrics, "src/frontend/" STRINGIFY(LANGUAGE) "/views/metrics.mst");
INCTXT(mqttIndex, "src/frontend/" STRINGIFY(LANGUAGE) "/views/mqtt/index.mst");
INCTXT(mqttTextField, "src/frontend/" STRINGIFY(LANGUAGE) "/views/mqtt/_text_field.mst");
INCTXT(others, "src/frontend/" STRINGIFY(LANGUAGE) "/views/others.mst");
INCTXT(upgrade, "src/frontend/" STRINGIFY(LANGUAGE) "/views/upgrade.mst");

INCTXT(css, "src/frontend/statics/mvp.css");

namespace partials {
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

const __FlashStringHelper *metrics = FPSTR(metricsData);

namespace mqtt {
const __FlashStringHelper *index = FPSTR(mqttIndexData);
const __FlashStringHelper *textField = FPSTR(mqttTextFieldData);
}  // namespace mqtt

const __FlashStringHelper *others = FPSTR(othersData);
const __FlashStringHelper *upgrade = FPSTR(upgradeData);
};  // namespace views

namespace statics {
const __FlashStringHelper *css = FPSTR(cssData);
}