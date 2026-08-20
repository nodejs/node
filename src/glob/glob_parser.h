#ifndef SRC_GLOB_GLOB_PARSER_H_
#define SRC_GLOB_GLOB_PARSER_H_

#include <string>
#include <string_view>
#include <vector>

#include "glob/glob_ast.h"

namespace node::glob {

// Sets *error and returns {} for a pattern that cannot be expanded.
std::vector<PatternString> BraceExpand(PatternView pattern,
                                       CompileError* error);

// Minimatch#slashSplit
template <typename Char, typename Emit>
void SlashSplitInto(std::basic_string_view<Char> s, bool windows, Emit emit) {
  if (windows && s.size() >= 3 && s[0] == '/' && s[1] == '/' && s[2] != '/') {
    emit(std::basic_string_view<Char>());
  }
  size_t i = 0;
  size_t start = 0;
  while (i < s.size()) {
    if (s[i] != '/') {
      i++;
      continue;
    }
    emit(s.substr(start, i - start));
    while (i < s.size() && s[i] == '/') i++;
    start = i;
  }
  emit(s.substr(start));
}

std::vector<PatternString> SlashSplit(PatternView s, const CompileFlags& flags);

// Minimatch#preprocess at optimizationLevel 2
void PreprocessLevel2(std::vector<std::vector<PatternString>>* glob_parts,
                      const CompileFlags& flags);

// Minimatch#levelTwoFileOptimize
template <typename Char>
void LevelTwoFileOptimize(std::vector<std::basic_string<Char>>* parts,
                          const CompileFlags& flags);

extern template void LevelTwoFileOptimize<char16_t>(
    std::vector<std::basic_string<char16_t>>*, const CompileFlags&);
extern template void LevelTwoFileOptimize<char8_t>(
    std::vector<std::basic_string<char8_t>>*, const CompileFlags&);

// AST.#parseAST + #flatten + #fillNegs
SegmentTree ParseSegmentAst(PatternView part, CompileError* error);

bool HasGlobMagic(PatternView s);

struct ParseResult {
  ParsedPattern pattern;
  CompileError error = CompileError::kNone;
};

ParseResult ParsePattern(PatternView raw, const CompileFlags& flags);

// Overwrites `out`, reusing its capacity across calls.
void Utf8ToUtf16(std::string_view utf8, PatternString* out);
std::string Utf16ToUtf8(PatternView utf16);

}  // namespace node::glob

#endif  // SRC_GLOB_GLOB_PARSER_H_
