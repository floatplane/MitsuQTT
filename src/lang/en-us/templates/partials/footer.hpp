#pragma once
#include <Arduino.h>

namespace partials {

const char *footer PROGMEM = R"====(
  <footer>
    Mitsubishi2MQTT build date <code>{{footer.version}}</code> revision <a href="https://github.com/floatplane/mitsubishi2MQTT/commit/{{footer.git_hash}}"><code>{{footer.git_hash}}</code></a>
  </footer>
</body>
</html>
)====";

};  // namespace partials
