// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_H_
#define V8_REGEXP_REGEXP_H_

#include "src/objects/js-regexp.h"

namespace v8 {
namespace internal {

class RegExpNode;
class RegExpTree;

// TODO(jgruber): Consider splitting between ParseData and CompileData.
struct RegExpCompileData {
  // The parsed AST as produced by the RegExpParser.
  RegExpTree* tree = nullptr;

  // The compiled Node graph as produced by RegExpTree::ToNode methods.
  RegExpNode* node = nullptr;

  // The generated code as produced by the compiler. Either a Code object (for
  // irregexp native code) or a ByteArray (for irregexp bytecode).
  Object code;

  // True, iff the pattern is a 'simple' atom with zero captures. In other
  // words, the pattern consists of a string with no metacharacters and special
  // regexp features, and can be implemented as a standard string search.
  bool simple = true;

  // True, iff the pattern is anchored at the start of the string with '^'.
  bool contains_anchor = false;

  // Only use if the pattern contains named captures. If so, this contains a
  // mapping of capture names to capture indices.
  Handle<FixedArray> capture_name_map;

  // The error message. Only used if an error occurred during parsing or
  // compilation.
  Handle<String> error;

  // The number of capture groups, without the global capture \0.
  int capture_count = 0;

  // The number of registers used by the generated code.
  int register_count = 0;
};

class RegExp final : public AllStatic {
 public:
  // Whether the irregexp engine generates native code or interpreter bytecode.
  static bool GeneratesNativeCode() { return !FLAG_regexp_interpret_all; }

  // Parses the RegExp pattern and prepares the JSRegExp object with
  // generic data and choice of implementation - as well as what
  // the implementation wants to store in the data field.
  // Returns false if compilation fails.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> Compile(
      Isolate* isolate, Handle<JSRegExp> re, Handle<String> pattern,
      JSRegExp::Flags flags);

  // See ECMA-262 section 15.10.6.2.
  // This function calls the garbage collector if necessary.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object> Exec(
      Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject,
      int index, Handle<RegExpMatchInfo> last_match_info);

  // Integral return values used throughout regexp code layers.
  static constexpr int kInternalRegExpFailure = 0;
  static constexpr int kInternalRegExpSuccess = 1;
  static constexpr int kInternalRegExpException = -1;
  static constexpr int kInternalRegExpRetry = -2;

  enum IrregexpResult {
    RE_FAILURE = kInternalRegExpFailure,
    RE_SUCCESS = kInternalRegExpSuccess,
    RE_EXCEPTION = kInternalRegExpException,
  };

  // Prepare a RegExp for being executed one or more times (using
  // IrregexpExecOnce) on the subject.
  // This ensures that the regexp is compiled for the subject, and that
  // the subject is flat.
  // Returns the number of integer spaces required by IrregexpExecOnce
  // as its "registers" argument.  If the regexp cannot be compiled,
  // an exception is set as pending, and this function returns negative.
  static int IrregexpPrepare(Isolate* isolate, Handle<JSRegExp> regexp,
                             Handle<String> subject);

  // Set last match info.  If match is nullptr, then setting captures is
  // omitted.
  static Handle<RegExpMatchInfo> SetLastMatchInfo(
      Isolate* isolate, Handle<RegExpMatchInfo> last_match_info,
      Handle<String> subject, int capture_count, int32_t* match);

  V8_EXPORT_PRIVATE static bool CompileForTesting(Isolate* isolate, Zone* zone,
                                                  RegExpCompileData* input,
                                                  JSRegExp::Flags flags,
                                                  Handle<String> pattern,
                                                  Handle<String> sample_subject,
                                                  bool is_one_byte);

  V8_EXPORT_PRIVATE static void DotPrintForTesting(const char* label,
                                                   RegExpNode* node);

  static const int kRegExpTooLargeToOptimize = 20 * KB;
};

// Uses a special global mode of irregexp-generated code to perform a global
// search and return multiple results at once. As such, this is essentially an
// iterator over multiple results (retrieved batch-wise in advance).
class RegExpGlobalCache final {
 public:
  RegExpGlobalCache(Handle<JSRegExp> regexp, Handle<String> subject,
                    Isolate* isolate);

  ~RegExpGlobalCache();

  // Fetch the next entry in the cache for global regexp match results.
  // This does not set the last match info.  Upon failure, nullptr is
  // returned. The cause can be checked with Result().  The previous result is
  // still in available in memory when a failure happens.
  int32_t* FetchNext();

  int32_t* LastSuccessfulMatch();

  bool HasException() { return num_matches_ < 0; }

 private:
  int AdvanceZeroLength(int last_index);

  int num_matches_;
  int max_matches_;
  int current_match_index_;
  int registers_per_match_;
  // Pointer to the last set of captures.
  int32_t* register_array_;
  int register_array_size_;
  Handle<JSRegExp> regexp_;
  Handle<String> subject_;
  Isolate* isolate_;
};

// Caches results for specific regexp queries on the isolate. At the time of
// writing, this is used during global calls to RegExp.prototype.exec and
// @@split.
class RegExpResultsCache final : public AllStatic {
 public:
  enum ResultsCacheType { REGEXP_MULTIPLE_INDICES, STRING_SPLIT_SUBSTRINGS };

  // Attempt to retrieve a cached result.  On failure, 0 is returned as a Smi.
  // On success, the returned result is guaranteed to be a COW-array.
  static Object Lookup(Heap* heap, String key_string, Object key_pattern,
                       FixedArray* last_match_out, ResultsCacheType type);
  // Attempt to add value_array to the cache specified by type.  On success,
  // value_array is turned into a COW-array.
  static void Enter(Isolate* isolate, Handle<String> key_string,
                    Handle<Object> key_pattern, Handle<FixedArray> value_array,
                    Handle<FixedArray> last_match_cache, ResultsCacheType type);
  static void Clear(FixedArray cache);

  static constexpr int kRegExpResultsCacheSize = 0x100;

 private:
  static constexpr int kStringOffset = 0;
  static constexpr int kPatternOffset = 1;
  static constexpr int kArrayOffset = 2;
  static constexpr int kLastMatchOffset = 3;
  static constexpr int kArrayEntriesPerCacheEntry = 4;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_H_
