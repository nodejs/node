#ifndef SRC_GLOB_GLOB_UNICODE_H_
#define SRC_GLOB_GLOB_UNICODE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "glob/glob_ast.h"

namespace node::glob {

// The last code point whose case mapping is pure ASCII arithmetic; above it
// case folding needs ICU (see Canonicalize).
inline constexpr uint32_t kMaxAsciiCodePoint = 0x7F;

// Avoiding util.h's ASCII + Locale work saves time, so this
// is a simple reimplementation.
inline constexpr uint32_t AsciiLower(uint32_t c) {
  return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}
inline constexpr uint32_t AsciiUpper(uint32_t c) {
  return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

enum class UnicodeProperty {
  kLetter,          // \p{L}
  kLetterNumber,    // \p{Nl}
  kDecimalNumber,   // \p{Nd}
  kSpaceSeparator,  // \p{Zs}
  kControl,         // \p{Cc}
  kSeparator,       // \p{Z}
  kOther,           // \p{C}
  kLowercase,       // \p{Ll}
  kUppercase,       // \p{Lu}
  kPunctuation,     // \p{P}
  kConnector,       // \p{Pc}
};

class CharSet {
 public:
  struct Range {
    uint32_t lo;
    uint32_t hi;
  };

  void AddRange(uint32_t lo, uint32_t hi);
  void AddChar(uint32_t cp) { AddRange(cp, cp); }

  // Adds every code point with the given property.
  bool AddProperty(UnicodeProperty property);

  bool CloseOverCaseFold();
  void CanonicalizeMembers();

  bool Contains(uint32_t cp) const;
  bool empty() const { return ranges_.empty(); }

  // Sorts and merges the ranges (so that we can optimize them!)
  void Seal();

  const std::vector<Range>& ranges() const { return ranges_; }

 private:
  std::vector<Range> ranges_;
};

uint32_t Canonicalize(uint32_t cp, bool fold);

// String.prototype.toLowerCase(), appended to `out`
void AppendLowered(const std::u16string& text, std::u16string* out);

}  // namespace node::glob

#endif  // SRC_GLOB_GLOB_UNICODE_H_
