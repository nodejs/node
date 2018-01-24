// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_REGEXP_H_
#define V8_BUILTINS_BUILTINS_REGEXP_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class RegExpBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit RegExpBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void BranchIfFastRegExp(Node* const context, Node* const object,
                          Node* const map, Label* const if_isunmodified,
                          Label* const if_ismodified);

 protected:
  // Allocate a RegExpResult with the given length (the number of captures,
  // including the match itself), index (the index where the match starts),
  // and input string. |length| and |index| are expected to be tagged, and
  // |input| must be a string.
  Node* AllocateRegExpResult(Node* context, Node* length, Node* index,
                             Node* input);

  Node* FastLoadLastIndex(Node* regexp);
  Node* SlowLoadLastIndex(Node* context, Node* regexp);
  Node* LoadLastIndex(Node* context, Node* regexp, bool is_fastpath);

  void FastStoreLastIndex(Node* regexp, Node* value);
  void SlowStoreLastIndex(Node* context, Node* regexp, Node* value);
  void StoreLastIndex(Node* context, Node* regexp, Node* value,
                      bool is_fastpath);

  // Loads {var_string_start} and {var_string_end} with the corresponding
  // offsets into the given {string_data}.
  void GetStringPointers(Node* const string_data, Node* const offset,
                         Node* const last_index, Node* const string_length,
                         String::Encoding encoding, Variable* var_string_start,
                         Variable* var_string_end);

  // Low level logic around the actual call into pattern matching code.
  Node* RegExpExecInternal(Node* const context, Node* const regexp,
                           Node* const string, Node* const last_index,
                           Node* const match_info);

  Node* ConstructNewResultFromMatchInfo(Node* const context, Node* const regexp,
                                        Node* const match_info,
                                        Node* const string);

  Node* RegExpPrototypeExecBodyWithoutResult(Node* const context,
                                             Node* const regexp,
                                             Node* const string,
                                             Label* if_didnotmatch,
                                             const bool is_fastpath);
  Node* RegExpPrototypeExecBody(Node* const context, Node* const regexp,
                                Node* const string, const bool is_fastpath);

  Node* ThrowIfNotJSReceiver(Node* context, Node* maybe_receiver,
                             MessageTemplate::Template msg_template,
                             char const* method_name);

  void BranchIfFastRegExp(Node* const context, Node* const object,
                          Label* const if_isunmodified,
                          Label* const if_ismodified);

  // Analogous to BranchIfFastRegExp, for use in asserts.
  Node* IsFastRegExp(Node* const context, Node* const object);

  // Performs fast path checks on the given object itself, but omits prototype
  // checks.
  Node* IsFastRegExpNoPrototype(Node* const context, Node* const object);
  Node* IsFastRegExpNoPrototype(Node* const context, Node* const object,
                                Node* const map);

  void BranchIfFastRegExpResult(Node* const context, Node* const object,
                                Label* if_isunmodified, Label* if_ismodified);

  Node* FlagsGetter(Node* const context, Node* const regexp, bool is_fastpath);

  Node* FastFlagGetter(Node* const regexp, JSRegExp::Flag flag);
  Node* SlowFlagGetter(Node* const context, Node* const regexp,
                       JSRegExp::Flag flag);
  Node* FlagGetter(Node* const context, Node* const regexp, JSRegExp::Flag flag,
                   bool is_fastpath);
  void FlagGetter(Node* context, Node* receiver, JSRegExp::Flag flag,
                  int counter, const char* method_name);

  Node* IsRegExp(Node* const context, Node* const maybe_receiver);
  Node* RegExpInitialize(Node* const context, Node* const regexp,
                         Node* const maybe_pattern, Node* const maybe_flags);

  Node* RegExpExec(Node* context, Node* regexp, Node* string);

  Node* AdvanceStringIndex(Node* const string, Node* const index,
                           Node* const is_unicode, bool is_fastpath);

  void RegExpPrototypeMatchBody(Node* const context, Node* const regexp,
                                Node* const string, const bool is_fastpath);

  void RegExpPrototypeSearchBodyFast(Node* const context, Node* const regexp,
                                     Node* const string);
  void RegExpPrototypeSearchBodySlow(Node* const context, Node* const regexp,
                                     Node* const string);

  void RegExpPrototypeSplitBody(Node* const context, Node* const regexp,
                                Node* const string, Node* const limit);

  Node* ReplaceGlobalCallableFastPath(Node* context, Node* regexp, Node* string,
                                      Node* replace_callable);
  Node* ReplaceSimpleStringFastPath(Node* context, Node* regexp, Node* string,
                                    Node* replace_string);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_REGEXP_H_
