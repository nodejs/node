// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_PRELUDE_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_PRELUDE_H_

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "experimental/redline/fast/tree.h"

namespace redline {

// JavaScript prepended to every program when it is executed (never part of
// the mutable program itself). The default prelude defines the mjsunit
// assertion API as non-throwing stubs so that imported mjsunit tests and
// their descendants keep running (and covering code) instead of failing on
// the first assertion.
std::string_view DefaultPrelude();

// Number of lines that `prelude` + "\n" adds before line 1 of a program.
int PreludeLineCount(std::string_view prelude);

// Why OnDemandPrelude::Select() gives the whole prelude to a program.
enum class PreludeFallback : uint8_t {
  kNone,        // The program gets only the statements that it can use.
  kEval,        // The text of the program has the token `eval`.
  kFunction,    // The text of the program has the token `Function`.
  kGlobalThis,  // The text of the program has the token `globalThis`.
  kRealm,       // The text of the program has the token `Realm`.
  kThis,        // `this` is a value, not only `this.name` or `this?.name`.
};
// The number of values of PreludeFallback. `kThis` is the last value.
inline constexpr int kNumPreludeFallbacks =
    static_cast<int>(PreludeFallback::kThis) + 1;
// The name of `fallback` in the statistics, for example "eval".
const char* PreludeFallbackName(PreludeFallback fallback);

// The default prelude as the list of its top-level statements (the tunable
// `prelude.on_demand`). A program gets only the statements that it can use,
// in the order of the prelude. Thus the target compiles and runs less code
// in front of each program. On demand works only with the default prelude
// (`DefaultPrelude()`, ignoring white space at the start and at the end).
// Each other prelude stays whole for each program.
//
// Build() finds the names that each statement declares and the names of the
// prelude that each statement uses. A program gets these statements:
// - The statements that declare a name that the text of the program has.
// - The statements that these statements use, directly or indirectly.
// - The statements that declare no name. They can change global state.
// - Any other statement that can change global state.
// A `var NAME = value` statement goes only with its names if the value has
// no effect. Such a value is a name, a property of a name, a function
// expression, or an object literal of these. Build() assumes that the read
// of a name or of a property runs no code. A statement `NAME.key = value`
// or `NAME.prototype.key = value` belongs to `NAME`, if `NAME` is a
// function expression of the prelude and the value has no effect. The two
// calls for `promiseTestChain` and `$262` also go only with their names,
// because they change no state that a program reads without the name. If
// you change the default prelude, run the whole-against-on-demand
// comparison on a corpus again.
//
// Select() reads the text of the program, not its syntax tree. A name in a
// string or in a comment counts, and Select() decodes escape sequences.
// Thus Select() finds each name that the program has in its text. A program
// can also compute the name of a global when it runs, or list the
// properties of the global object. The fallback rules (PreludeFallback)
// give the whole prelude to a program that can do this in a usual way.
// They do not find each way. DESIGN.md gives the known limits.
class OnDemandPrelude {
 public:
  // The most top-level statements of a prelude that Build() accepts.
  static constexpr size_t kMaxStatements = 256;
  using Mask = std::bitset<kMaxStatements>;

  // The part of the prelude for one program.
  struct Selection {
    Mask statements;   // the top-level statements of the prelude, by index
    size_t count = 0;  // the number of these statements
    size_t bytes = 0;  // the size of the text
    int lines = 0;     // PreludeLineCount() of the text
    PreludeFallback fallback = PreludeFallback::kNone;
  };

  // Splits the default prelude (white space at the start and at the end
  // does not count). Without `fallback_rules`, Select() gives the whole
  // prelude to no program. Returns nullptr and sets `error` for each other
  // prelude, or if Build() cannot split the prelude.
  static std::unique_ptr<OnDemandPrelude> Build(std::string_view prelude,
                                                bool fallback_rules,
                                                std::string* error);

  // Registers a text that Redline puts in front of a program, for example
  // the runtime of a runtime-assisted mutator. The fallback rules do not
  // apply to this text, because it is not a part of the program. Call it
  // before the first Select() of another thread.
  void AddHelper(std::string_view helper);

  // The part of the prelude for `program`.
  Selection Select(std::string_view program) const;
  // Appends the text of `selection` to `out`. `selection` must have at
  // least one statement, because the default prelude always keeps
  // statements in front of each program.
  void AppendText(const Selection& selection, std::string* out) const;
  std::string Text(const Selection& selection) const;

  // The number of top-level statements of the prelude.
  size_t statements() const { return newlines_.size(); }
  // The number of statements that each program gets.
  size_t always_kept() const { return always_.count(); }
  // True if Select() can give the whole prelude (the fallback rules).
  bool fallback_rules() const { return fallback_rules_; }

 private:
  struct Token {
    Mask statements;  // the statements of a prelude name
    PreludeFallback fallback = PreludeFallback::kNone;
  };
  struct Helper {
    std::string text;
    Mask statements;
  };

  OnDemandPrelude() = default;
  // Adds the statements of the prelude names in `text` to `statements`.
  // With `rules`, returns the first fallback rule that applies.
  PreludeFallback Scan(std::string_view text, bool rules,
                       Mask* statements) const;
  Selection MakeSelection(const Mask& statements) const;

  std::string text_;
  // Statement i is text_[offsets_[i], offsets_[i + 1]), with the white space
  // and the comments up to the next statement. It has newlines_[i] new
  // lines.
  std::vector<size_t> offsets_;
  std::vector<int> newlines_;
  Mask always_;
  bool fallback_rules_ = false;
  Selection whole_;
  absl::flat_hash_map<std::string, Token> tokens_;
  // Build() sets bit n of `lengths_[c]` if a token starts with `c` and has
  // n bytes (63 or more bytes for bit 63). Scan() looks up only these
  // tokens.
  uint64_t lengths_[256] = {};
  std::vector<Helper> helpers_;
};

// The on-demand prelude for `prelude` if the tunable `prelude.on_demand` is
// on. Returns nullptr if the tunable is off or `prelude` is empty. Also
// returns nullptr, with a warning, if `prelude` is not the default prelude
// or Build() cannot split it. Then each program gets the whole prelude. The
// runtime of each runtime-assisted mutator is a helper of the result
// (AddHelper). Thus a session and the commands make the same script for the
// same text.
std::unique_ptr<OnDemandPrelude> MakeOnDemandPrelude(std::string_view prelude);

// The script that the target runs for `program`. Without a prelude, it is
// the program. Else it is the prelude, a new line and the program. With
// `on_demand` (can be nullptr), it has only the part of the prelude for the
// program, and `selection` (can be nullptr) gets this part.
std::string ScriptWithPrelude(std::string_view prelude,
                              const OnDemandPrelude* on_demand,
                              std::string_view program,
                              OnDemandPrelude::Selection* selection = nullptr);

// The script that the target runs for a program, the prelude, a new line and
// the program, parsed into one tree. The crash minimizer reduces this tree.
// Thus it can also remove the prelude statements that a crash does not need
// (DESIGN.md 5.6). The tree prints to a script that is equivalent to the
// script of the target, but not always the same text.
class ScriptTree {
 public:
  // Parses `prelude` + "\n" + `program`, or only `program` if `prelude` is
  // empty. Returns nullptr and sets `error` if the prelude or the program
  // does not parse alone. Also if the top-level statements of the script are
  // not the statements of the prelude, then the statements of the program.
  static std::unique_ptr<ScriptTree> Parse(std::string_view prelude,
                                           std::string_view program,
                                           std::string* error);

  fast::Tree* tree() { return tree_.get(); }
  const fast::Tree* tree() const { return tree_.get(); }
  // The top-level statements of the prelude in the parsed script.
  size_t prelude_statements() const { return prelude_statements_; }
  // The number of top-level statements of the tree that came from the
  // prelude, at the time of the call.
  size_t PreludeStatementsNow() const;
  // The number of nodes of the program part of the tree: the root and the
  // top-level statements that came from the program, with their subtrees.
  size_t ProgramNodes() const;
  // Prints the program part of the tree. A statement that a reducer made
  // counts as a program statement.
  std::string PrintProgram();

 private:
  ScriptTree() = default;
  bool FromPrelude(const fast::Node* statement) const {
    return statement->id < first_program_id_;
  }

  std::unique_ptr<fast::Tree> tree_;
  size_t prelude_statements_ = 0;
  // The parser gives the ids in the order of the source text, and the root
  // has id 0. The nodes of the prelude have lower ids than this id. The
  // nodes of the program have this id or higher ids.
  uint32_t first_program_id_ = 1;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_PRELUDE_H_
