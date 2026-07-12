#include "embedded_data.h"
#include <array>

namespace node {
namespace {

struct OctalStr {
  char data[5] = {0};

  constexpr OctalStr() = default;
  explicit constexpr OctalStr(uint8_t ch) {
    // We can print most printable characters directly. The exceptions are '\'
    // (escape characters), " (would end the string), and ? (trigraphs). The
    // latter may be overly conservative: we compile with C++20 which doesn't
    // support trigraphs.
    if (ch >= ' ' && ch <= '~' && ch != '\\' && ch != '"' && ch != '?') {
      data[0] = static_cast<char>(ch);
    } else {
      data[0] = '\\';
      data[1] = '0' + ((ch >> 6) & 7);
      data[2] = '0' + ((ch >> 3) & 7);
      data[3] = '0' + (ch & 7);
    }
  }

  constexpr std::string_view view() const {
    return {data, data[1] == '\0' ? 1u : 4u};
  }
};

constexpr auto MakeOctalTable() {
  std::array<OctalStr, 256> table{};
  for (unsigned i = 0; i < 256; ++i) {
    table[i] = OctalStr(static_cast<uint8_t>(i));
  }
  return table;
}

constexpr auto octal_table = MakeOctalTable();

}  // namespace

std::string_view GetOctalCode(uint8_t index) {
  return octal_table[index].view();
}

}  // namespace node
