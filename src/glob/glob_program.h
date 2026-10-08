#ifndef SRC_GLOB_GLOB_PROGRAM_H_
#define SRC_GLOB_GLOB_PROGRAM_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "glob/glob_ast.h"
#include "glob/glob_unicode.h"

namespace node::glob {

struct RxNode {
  enum class Kind : uint8_t {
    kEmpty,    // matches ''
    kAssert,   // zero-width start guard (see assert_* flags)
    kText,     // literal unit run
    kAnyChar,  // [^/]
    kStarAny,  // [^/]*?  (flags below select +? and dot guards)
    kClass,
    kConcat,
    kAlt,
    kOpt,      // (?:body)?
    kStar,     // (?:body)*
    kPlus,     // (?:body)+
    kTwoBody,  // (?:first)(?:rest)*?   children = [first, rest]
    kNeg,      // !(..) group: guard alts + [^/]*? consume
  };
  Kind kind = Kind::kEmpty;
  bool star_no_empty = false;        // at least one unit
  bool star_no_dot = false;          // (?!\.) at entry
  bool assert_no_dot = false;        // `(?!\.)`
  bool assert_no_traversal = false;  // `(?!(?:^|/)\.\.?(?:$|/))`

  struct CharClass {
    CharSet positive;
    CharSet negative;
    bool has_positive = false;
    bool has_negative = false;
    bool negate = false;
  };
  CharClass klass;
  PatternString text;
  std::vector<uint32_t> children;
};

// One compiled path segment.
struct SegmentProgram {
  enum class Tier : uint8_t {
    kEmpty,        // '', which matches only ''
    kLiteral,      // f === text
    kStar,         // /^\*+$/
    kStarDotExt,   // /^\*+([^+@!?*[(]*)$/
    kStarDotStar,  // /^\*+\.\*+$/
    kDotStar,      // /^\.\*+$/
    kQmarks,       // /^\?+([^+@!?*[(]*)?$/
    kRegex,        // generic
  };
  Tier tier = Tier::kRegex;
  bool uflag = false;   // code-point semantics + 'iu' folding
  bool nocase = false;  // fold case (kLiteral: only if !nocase_magic_only)
  bool dot = false;

  // If kLiteral: the text. If kStarDotExt/kQmarks: the extension.
  PatternString literal;
  PatternString literal_lower;

  // kQmarks: full pattern length in UTF-16 units ($0.length).
  size_t qmarks_len = 0;

  // kRegex:
  std::vector<RxNode> nodes;
  uint32_t root = 0;
  bool start_no_dot = false;        // (?!\.)
  bool start_no_traversal = false;  // rejects '.' and '..'
  bool invalid_regex = false;
};

// One lowered path part.
struct PartMatcher {
  enum class Kind : uint8_t { kGlobstar, kLiteral, kProgram };
  Kind kind = Kind::kProgram;
  PatternString literal;  // kLiteral: '===' comparison (Windows roots etc.)

  // Source info
  PatternString source;
  SegmentProgram program;
};

struct CompiledPattern {
  bool empty = false;  // empty pattern: matches only ''
  CompileFlags flags;
  struct Row {
    std::vector<PartMatcher> parts;
    bool has_globstar = false;
  };
  std::vector<Row> rows;

  // Set when the pattern does not compile
  CompileError error = CompileError::kNone;
};

using CompiledPatternPtr = std::shared_ptr<CompiledPattern>;
CompiledPattern CompileProgram(ParsedPattern&& parsed,
                               const CompileFlags& flags);

// "Lowering", as in toLowerCase-ing
SegmentProgram LowerSegment(SegmentNode* root,
                            const CompileFlags& flags,
                            PatternView source);

}  // namespace node::glob

#endif  // SRC_GLOB_GLOB_PROGRAM_H_
