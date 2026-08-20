#ifndef SRC_GLOB_GLOB_AST_H_
#define SRC_GLOB_GLOB_AST_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace node::glob {

using PatternString = std::u16string;
using PatternView = std::u16string_view;

struct CompileFlags {
  bool windows = false;
  bool nocase = false;
  bool dot = false;
  bool match_base = false;
};

constexpr bool IsAsciiLetter(uint32_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

constexpr bool IsLineTerminator(uint32_t c) {
  return c == '\n' || c == '\r' || c == 0x2028 || c == 0x2029;
}

template <typename A, typename B>
bool UnitsEqual(std::basic_string_view<A> a, std::basic_string_view<B> b) {
  return std::ranges::equal(a, b, [](A x, B y) {
    return static_cast<uint32_t>(x) == static_cast<uint32_t>(y);
  });
}

template <typename Char>
bool StartsWithWindowsDrive(std::basic_string_view<Char> s) {
  return s.size() >= 2 && s[1] == ':' && IsAsciiLetter(s[0]);
}
template <typename Char>
bool IsWindowsDrive(std::basic_string_view<Char> s) {
  return s.size() == 2 && StartsWithWindowsDrive(s);
}

inline constexpr size_t kMaxPatternLength = 64 * 1024;
inline constexpr int kMaxExtglobRecursion = 2;
inline constexpr int kMaxGlobstarRecursion = 200;
inline constexpr size_t kBraceExpansionMax = 100'000;
inline constexpr size_t kBraceExpansionMaxLength = 4'000'000;

enum class CompileError {
  kNone,
  kPatternTooLong,
  kInvalidRegExp,
};

// One node of the per-segment extglob AST, ported from minimatch's AST
// class (ast.js)
struct SegmentNode {
  struct Part {
    // Exactly one of these is meaningful: `child` when non-null, else `text`.
    PatternString text;
    SegmentNode* child = nullptr;

    Part() = default;
    explicit Part(PatternString t) : text(std::move(t)) {}
    explicit Part(SegmentNode* c) : child(c) {}
    bool is_child() const { return child != nullptr; }
  };
  using Parts = std::vector<Part>;

  char type = 0;

  std::shared_ptr<Parts> parts = std::make_shared<Parts>();
  // Set when an extglob closed with no contents: `!()`, `@()`, ...
  bool empty_ext = false;

  // Tree links used by flatten and fill-negs
  SegmentNode* parent = nullptr;
  size_t parent_index = 0;

  bool is_extglob() const { return type != 0; }

  // isStart() / isEnd() from Minimatch's ast.js
  bool IsStart() const;
  bool IsEnd() const;

  // AST.toString()
  PatternString ToString() const;
};

struct SegmentTree {
  SegmentNode* root = nullptr;
  std::vector<std::unique_ptr<SegmentNode>> arena;
};

struct PathPart {
  enum class Kind : uint8_t { kGlobstar, kSegment };
  Kind kind = Kind::kSegment;
  PatternString source;
  SegmentTree segment;
};

struct ParsedPattern {
  struct Row {
    std::vector<PathPart> parts;
  };
  std::vector<Row> rows;
  bool empty = false;
};

}  // namespace node::glob

#endif  // SRC_GLOB_GLOB_AST_H_
