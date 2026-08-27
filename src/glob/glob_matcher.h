#ifndef SRC_GLOB_GLOB_MATCHER_H_
#define SRC_GLOB_GLOB_MATCHER_H_

#include <string_view>

#include "glob/glob_program.h"

namespace node::glob {

using OneByteView = std::basic_string_view<char8_t>;

bool TestPart(const PartMatcher& part, PatternView f);
bool TestPart(const PartMatcher& part, OneByteView f);

bool MatchPattern(const CompiledPattern& pattern, PatternView path);
bool MatchPattern(const CompiledPattern& pattern, OneByteView path);

}  // namespace node::glob

#endif  // SRC_GLOB_GLOB_MATCHER_H_
