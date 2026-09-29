#pragma once
#include <string>
#include "LIEF/optional.hpp"

namespace LIEF::runtime::linux_android {
optional<std::string> cmdline();
}
