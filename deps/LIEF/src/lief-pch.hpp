#include <LIEF/config.h>

#if defined __cplusplus
  #include "spdlog/spdlog.h"
  #include <spdlog/fmt/fmt.h>
  #include <spdlog/fmt/ranges.h>

  #include <string>
  #include <vector>
  #include <set>
  #include <map>
  #include <unordered_map>
  #include <array>
  #include <memory>
  #include <algorithm>
  #include <cstdint>
  #include <cstddef>
  #include <ostream>
  #include <sstream>

  #if LIEF_JSON_SUPPORT
    #if defined(__clang__)
      #pragma clang diagnostic push
      #pragma clang diagnostic ignored "-Wunknown-warning-option"
      #pragma clang diagnostic ignored "-Wlifetime-safety"
    #endif
    #ifndef LIEF_NLOHMANN_JSON_EXTERNAL
      #include "internal/nlohmann/json.hpp"
    #else
      #include <nlohmann/json.hpp>
    #endif
    #if defined(__clang__)
      #pragma clang diagnostic pop
    #endif
  #endif
#else
  #include <stdint.h>
  #include <stddef.h>
#endif
