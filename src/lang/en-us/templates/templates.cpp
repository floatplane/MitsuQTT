#include "templates.hpp"

#define INCBIN_PREFIX
#include "incbin.h"

INCTXT(footer, "src/lang/en-us/templates/partials/footer.mst");
INCTXT(header, "src/lang/en-us/templates/partials/header.mst");
namespace partials {
const __FlashStringHelper *footer = footerData;
const __FlashStringHelper *header = headerData;
};  // namespace partials

INCTXT(autoconfig, "src/lang/en-us/templates/views/autoconfig.mst");
namespace views {
const __FlashStringHelper *autoconfig = autoconfigData;
};  // namespace views
