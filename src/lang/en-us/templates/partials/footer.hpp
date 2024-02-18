#pragma once
#include <Arduino.h>

namespace partials {

const char *footer PROGMEM = R"====(
    <br/>
    <div style='text-align:right;font-size:10px;color: grey;'>
      <hr/>
      Mitsubishi2MQTT {{footer.version}} ({{footer.git_hash}})
    </div>
  </div>
</body>
</html>
)====";

};  // namespace partials
