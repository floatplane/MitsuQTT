#include "templates.hpp"

#define INCBIN_PREFIX
#include "incbin.h"

#define STRINGIFY_1(s) #s
#define STRINGIFY(s) STRINGIFY_1(s)

INCTXT(footer, "src/frontend/" STRINGIFY(LANGUAGE) "/partials/footer.mst");
INCTXT(header, "src/frontend/" STRINGIFY(LANGUAGE) "/partials/header.mst");

namespace partials {
const __FlashStringHelper *footer = footerData;
const __FlashStringHelper *header = headerData;
};  // namespace partials

INCTXT(autoconfig, "src/frontend/" STRINGIFY(LANGUAGE) "/views/autoconfig.mst");
INCTXT(others, "src/frontend/" STRINGIFY(LANGUAGE) "/views/others.mst");
namespace views {
const __FlashStringHelper *autoconfig = autoconfigData;
const __FlashStringHelper *others = othersData;
};  // namespace views

INCTXT(css, "src/frontend/statics/mvp.css");
namespace statics {
const __FlashStringHelper *css = cssData;
}