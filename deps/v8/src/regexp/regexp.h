// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_H_
#define V8_REGEXP_REGEXP_H_

#include "src/common/assert-scope.h"
#include "src/handles/handles.h"
#include "src/regexp/regexp-error.h"
#include "src/regexp/regexp-flags.h"
#include "src/regexp/regexp-result-vector.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class JSRegExp;
class RegExpData;
class IrRegExpData;
class AtomRegExpData;
class RegExpMatchInfo;
class TrustedFixedArray;

namespace regexp {

class Capture;
class Node;
class Tree;

enum class CompilationTarget : int { kBytecode, kNative };

// TODO(jgruber): Do not expose in regexp.h.
// TODO(jgruber): Consider splitting between ParseData and CompileData.
struct CompileData {
  // The parsed AST as produced by the Parser.
  Tree* tree = nullptr;

  // The compiled Node graph as produced by Tree::ToNode methods.
  Node* node = nullptr;

  // Either the generated code as produced by the compiler or a trampoline
  // to the interpreter.
  DirectHandle<Object> code;

  // True, iff the pattern is a 'simple' atom with zero captures. In other
  // words, the pattern consists of a string with no metacharacters and special
  // regexp features, and can be implemented as a standard string search.
  bool simple = true;

  // True, iff the pattern is anchored at the start of the string with '^'.
  bool contains_anchor = false;

  // Only set if the pattern contains named captures.
  // Note: the lifetime equals that of the parse/compile zone.
  ZoneVector<Capture*>* named_captures = nullptr;

  // The error message. Only used if an error occurred during parsing or
  // compilation.
  Error error = Error::kNone;

  // The position at which the error was detected. Only used if an
  // error occurred.
  int error_pos = 0;

  // The number of capture groups, without the global capture \0.
  int capture_count = 0;

  // The number of registers used by the generated code.
  int register_count = 0;

  // The compilation target (bytecode or native code).
  CompilationTarget compilation_target;
};

}  // namespace regexp

class RegExp final : public AllStatic {
 public:
  // Whether the irregexp engine generates interpreter bytecode.
  static bool CanGenerateBytecode();

  // Verify that the given flags combination is valid.
  V8_EXPORT_PRIVATE static bool VerifyFlags(regexp::Flags flags);

  // Verify the given pattern, i.e. check that parsing succeeds. If
  // verification fails, `regexp_error_out` is set.
  template <class CharT>
  static bool VerifySyntax(Zone* zone, uintptr_t stack_limit,
                           const CharT* input, int input_length,
                           regexp::Flags flags, regexp::Error* regexp_error_out,
                           const DisallowGarbageCollection& no_gc);

  // Parses the RegExp pattern and prepares the JSRegExp object with
  // generic data and choice of implementation - as well as what
  // the implementation wants to store in the data field.
  // Returns false if compilation fails.
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Object> Compile(
      Isolate* isolate, DirectHandle<JSRegExp> re,
      DirectHandle<String> original_source, regexp::Flags flags,
      uint32_t backtrack_limit);

  // Ensures that a regexp is fully compiled and ready to be executed on a
  // subject string.  Returns true on success. Throw and return false on
  // failure.
  V8_WARN_UNUSED_RESULT static bool EnsureFullyCompiled(
      Isolate* isolate, DirectHandle<RegExpData> re_data,
      DirectHandle<String> subject);

  enum CallOrigin : int {
    kFromRuntime = 0,
    kFromJs = 1,
  };

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS
  static void TraceExecutionBegin(Address isolate_ptr);
  static void TraceExecutionEnd(Address isolate_ptr, Address data_ptr,
                                Address subject_ptr, int32_t last_index,
                                int32_t result);
#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS

  // See ECMA-262 section 15.10.6.2.
  // This function calls the garbage collector if necessary.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static std::optional<int> Exec(
      Isolate* isolate, DirectHandle<JSRegExp> regexp,
      DirectHandle<String> subject, int index, int32_t* result_offsets_vector,
      uint32_t result_offsets_vector_length);
  // As above, but passes the result through the old-style RegExpMatchInfo|Null
  // interface. At most one match is returned.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Object>
  Exec_Single(Isolate* isolate, DirectHandle<JSRegExp> regexp,
              DirectHandle<String> subject, int index,
              DirectHandle<RegExpMatchInfo> last_match_info);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static std::optional<int>
  ExperimentalOneshotExec(Isolate* isolate, DirectHandle<JSRegExp> regexp,
                          DirectHandle<String> subject, int index,
                          int32_t* result_offsets_vector,
                          uint32_t result_offsets_vector_length);

  // Called directly from generated code through ExternalReference.
  V8_EXPORT_PRIVATE static intptr_t AtomExecRaw(
      Isolate* isolate, Address /* AtomRegExpData */ data_address,
      Address /* String */ subject_address, int32_t index,
      int32_t* result_offsets_vector, int32_t result_offsets_vector_length);

  // Integral return values used throughout regexp code layers.
  static constexpr int kInternalRegExpFailure = 0;
  static constexpr int kInternalRegExpSuccess = 1;
  static constexpr int kInternalRegExpException = -1;
  static constexpr int kInternalRegExpRetry = -2;
  static constexpr int kInternalRegExpFallbackToExperimental = -3;
  static constexpr int kInternalRegExpSmallestResult = -3;

  enum IrregexpResult : int32_t {
    RE_FAILURE = kInternalRegExpFailure,
    RE_SUCCESS = kInternalRegExpSuccess,
    RE_EXCEPTION = kInternalRegExpException,
    RE_RETRY = kInternalRegExpRetry,
    RE_FALLBACK_TO_EXPERIMENTAL = kInternalRegExpFallbackToExperimental,
  };

  // Set last match info.  If match is nullptr, then setting captures is
  // omitted.
  static DirectHandle<RegExpMatchInfo> SetLastMatchInfo(
      Isolate* isolate, DirectHandle<RegExpMatchInfo> last_match_info,
      DirectHandle<String> subject, int capture_count, int32_t* match);

  V8_EXPORT_PRIVATE static bool CompileForTesting(
      Isolate* isolate, Zone* zone, regexp::CompileData* input,
      regexp::Flags flags, DirectHandle<String> pattern,
      DirectHandle<String> sample_subject, DirectHandle<IrRegExpData> re_data,
      bool is_one_byte);

  V8_EXPORT_PRIVATE static void DotPrintForTesting(const char* label,
                                                   regexp::Node* node);

  static const int kMaxOptimizedPatternLength = 20 * KB;

  V8_WARN_UNUSED_RESULT
  static MaybeDirectHandle<Object> ThrowRegExpException(
      Isolate* isolate, regexp::Flags flags,
      DirectHandle<String> original_source, regexp::Error error);
  static void ThrowRegExpException(Isolate* isolate,
                                   DirectHandle<RegExpData> re_data,
                                   regexp::Error error_text);

  static bool IsUnmodifiedRegExp(Isolate* isolate,
                                 DirectHandle<JSRegExp> regexp);

  static DirectHandle<TrustedFixedArray> CreateCaptureNameMap(
      Isolate* isolate, ZoneVector<regexp::Capture*>* named_captures);
};

namespace regexp {

// Uses a special global mode of irregexp-generated code to perform a global
// search and return multiple results at once. As such, this is essentially an
// iterator over multiple results (retrieved batch-wise in advance).
class GlobalExecRunner final {
 public:
  GlobalExecRunner(DirectHandle<RegExpData> regexp_data,
                   DirectHandle<String> subject, Isolate* isolate);

  // Fetch the next entry in the cache for global regexp match results.
  // This does not set the last match info.  Upon failure, nullptr is
  // returned. The cause can be checked with Result().  The previous result is
  // still in available in memory when a failure happens.
  int32_t* FetchNext();

  int32_t* LastSuccessfulMatch() const;

  bool HasException() const { return num_matches_ < 0; }

 private:
  int AdvanceZeroLength(int last_index) const;

  int max_matches() const {
    DCHECK_NE(register_array_size_, 0);
    return register_array_size_ / registers_per_match_;
  }

  ResultVectorScope result_vector_scope_;
  int num_matches_ = 0;
  int current_match_index_ = 0;
  int registers_per_match_ = 0;
  // Pointer to the last set of captures.
  int32_t* register_array_ = nullptr;
  int register_array_size_ = 0;
  DirectHandle<RegExpData> regexp_data_;
  DirectHandle<String> subject_;
  Isolate* const isolate_;
};

// Caches results for specific regexp queries on the isolate. At the time of
// writing, this is used during global calls to RegExp.prototype.exec and
// @@split.
class ResultsCache final : public AllStatic {
 public:
  enum ResultsCacheType { REGEXP_MULTIPLE_INDICES, STRING_SPLIT_SUBSTRINGS };

  // Attempt to retrieve a cached result.  On failure, 0 is returned as a Smi.
  // On success, the returned result is guaranteed to be a COW-array.
  static Tagged<Object> Lookup(Heap* heap, Tagged<String> key_string,
                               Tagged<Object> key_pattern,
                               Tagged<FixedArray>* last_match_out,
                               ResultsCacheType type);
  // Attempt to add value_array to the cache specified by type.  On success,
  // value_array is turned into a COW-array.
  static void Enter(Isolate* isolate, DirectHandle<String> key_string,
                    DirectHandle<Object> key_pattern,
                    DirectHandle<FixedArray> value_array,
                    DirectHandle<FixedArray> last_match_cache,
                    ResultsCacheType type);
  static void Clear(Tagged<FixedArray> cache);

  static constexpr int kRegExpResultsCacheSize = 0x100;

 private:
  static constexpr int kStringOffset = 0;
  static constexpr int kPatternOffset = 1;
  static constexpr int kArrayOffset = 2;
  static constexpr int kLastMatchOffset = 3;
  static constexpr int kArrayEntriesPerCacheEntry = 4;
};

// Caches results of RegExpPrototypeMatch when:
// - the subject is a SlicedString
// - the pattern is an ATOM type regexp.
//
// This is intended for usage patterns where we search ever-growing slices of
// some large string. After a cache hit, RegExpMatchGlobalAtom only needs to
// process the trailing part of the subject string that was *not* part of the
// cached SlicedString.
//
// For example:
//
// long_string.substring(0, 100).match(pattern);
// long_string.substring(0, 200).match(pattern);
//
// The second call hits the cache for the slice [0, 100[ and only has to search
// the slice [100, 200].
class ResultsCache_MatchGlobalAtom final : public AllStatic {
 public:
  static void TryInsert(Isolate* isolate, Tagged<String> subject,
                        Tagged<String> pattern, uint32_t number_of_matches,
                        int last_match_index);
  static bool TryGet(Isolate* isolate, Tagged<String> subject,
                     Tagged<String> pattern, uint32_t* number_of_matches_out,
                     int* last_match_index_out);
  static void Clear(Heap* heap);

 private:
  static constexpr int kSubjectIndex = 0;          // SlicedString.
  static constexpr int kPatternIndex = 1;          // String.
  static constexpr int kNumberOfMatchesIndex = 2;  // Smi.
  static constexpr int kLastMatchIndexIndex = 3;   // Smi.
  static constexpr int kEntrySize = 4;

 public:
  static constexpr int kSize = kEntrySize;  // Single-entry cache.
};

}  // namespace regexp
}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_H_
