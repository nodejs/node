// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_REGEXP_GEN_H_
#define V8_BUILTINS_BUILTINS_REGEXP_GEN_H_

#include "src/base/optional.h"
#include "src/code-stub-assembler.h"
#include "src/message-template.h"

namespace v8 {
namespace internal {

class RegExpBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit RegExpBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void BranchIfFastRegExp(
      Node* const context, Node* const object, Node* const map,
      base::Optional<DescriptorIndexAndName> additional_property_to_check,
      Label* const if_isunmodified, Label* const if_ismodified);

  // Create and initialize a RegExp object.
  TNode<Object> RegExpCreate(TNode<Context> context,
                             TNode<Context> native_context,
                             TNode<Object> regexp_string, TNode<String> flags);

  TNode<Object> RegExpCreate(TNode<Context> context, TNode<Map> initial_map,
                             TNode<Object> regexp_string, TNode<String> flags);

 protected:
  TNode<Smi> SmiZero();
  TNode<IntPtrT> IntPtrZero();

  // Allocate a RegExpResult with the given length (the number of captures,
  // including the match itself), index (the index where the match starts),
  // and input string.
  TNode<JSRegExpResult> AllocateRegExpResult(TNode<Context> context,
                                             TNode<Smi> length,
                                             TNode<Smi> index,
                                             TNode<String> input);

  TNode<Object> FastLoadLastIndex(TNode<JSRegExp> regexp);
  TNode<Object> SlowLoadLastIndex(TNode<Context> context, TNode<Object> regexp);
  TNode<Object> LoadLastIndex(TNode<Context> context, TNode<Object> regexp,
                              bool is_fastpath);

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
  TNode<HeapObject> RegExpExecInternal(TNode<Context> context,
                                       TNode<JSRegExp> regexp,
                                       TNode<String> string,
                                       TNode<Number> last_index,
                                       TNode<RegExpMatchInfo> match_info);

  TNode<JSRegExpResult> ConstructNewResultFromMatchInfo(
      TNode<Context> context, TNode<JSReceiver> maybe_regexp,
      TNode<RegExpMatchInfo> match_info, TNode<String> string);

  TNode<RegExpMatchInfo> RegExpPrototypeExecBodyWithoutResult(
      TNode<Context> context, TNode<JSReceiver> maybe_regexp,
      TNode<String> string, Label* if_didnotmatch, const bool is_fastpath);
  TNode<HeapObject> RegExpPrototypeExecBody(TNode<Context> context,
                                            TNode<JSReceiver> maybe_regexp,
                                            TNode<String> string,
                                            const bool is_fastpath);

  Node* ThrowIfNotJSReceiver(Node* context, Node* maybe_receiver,
                             MessageTemplate msg_template,
                             char const* method_name);

  // Analogous to BranchIfFastRegExp, for use in asserts.
  TNode<BoolT> IsFastRegExp(SloppyTNode<Context> context,
                            SloppyTNode<Object> object);

  void BranchIfFastRegExp(Node* const context, Node* const object,
                          Label* const if_isunmodified,
                          Label* const if_ismodified);

  // Performs fast path checks on the given object itself, but omits prototype
  // checks.
  Node* IsFastRegExpNoPrototype(Node* const context, Node* const object);
  TNode<BoolT> IsFastRegExpWithOriginalExec(TNode<Context> context,
                                            TNode<JSRegExp> object);
  Node* IsFastRegExpNoPrototype(Node* const context, Node* const object,
                                Node* const map);

  void BranchIfFastRegExpResult(Node* const context, Node* const object,
                                Label* if_isunmodified, Label* if_ismodified);

  Node* FlagsGetter(Node* const context, Node* const regexp, bool is_fastpath);

  TNode<Int32T> FastFlagGetter(TNode<JSRegExp> regexp, JSRegExp::Flag flag);
  TNode<Int32T> SlowFlagGetter(TNode<Context> context, TNode<Object> regexp,
                               JSRegExp::Flag flag);
  TNode<Int32T> FlagGetter(TNode<Context> context, TNode<Object> regexp,
                           JSRegExp::Flag flag, bool is_fastpath);

  void FlagGetter(Node* context, Node* receiver, JSRegExp::Flag flag,
                  int counter, const char* method_name);

  TNode<BoolT> IsRegExp(TNode<Context> context, TNode<Object> maybe_receiver);

  Node* RegExpInitialize(Node* const context, Node* const regexp,
                         Node* const maybe_pattern, Node* const maybe_flags);

  Node* RegExpExec(Node* context, Node* regexp, Node* string);

  Node* AdvanceStringIndex(Node* const string, Node* const index,
                           Node* const is_unicode, bool is_fastpath);

  void RegExpPrototypeMatchBody(Node* const context, Node* const regexp,
                                TNode<String> const string,
                                const bool is_fastpath);

  void RegExpPrototypeSearchBodyFast(Node* const context, Node* const regexp,
                                     Node* const string);
  void RegExpPrototypeSearchBodySlow(Node* const context, Node* const regexp,
                                     Node* const string);

  void RegExpPrototypeSplitBody(Node* const context, Node* const regexp,
                                TNode<String> const string,
                                TNode<Smi> const limit);

  Node* ReplaceGlobalCallableFastPath(Node* context, Node* regexp, Node* string,
                                      Node* replace_callable);
  Node* ReplaceSimpleStringFastPath(Node* context, Node* regexp,
                                    TNode<String> string,
                                    TNode<String> replace_string);
};

class RegExpMatchAllAssembler : public RegExpBuiltinsAssembler {
 public:
  explicit RegExpMatchAllAssembler(compiler::CodeAssemblerState* state)
      : RegExpBuiltinsAssembler(state) {}

  TNode<Object> CreateRegExpStringIterator(TNode<Context> native_context,
                                           TNode<Object> regexp,
                                           TNode<String> string,
                                           TNode<Int32T> global,
                                           TNode<Int32T> full_unicode);
  void Generate(TNode<Context> context, TNode<Context> native_context,
                TNode<Object> receiver, TNode<Object> maybe_string);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_REGEXP_GEN_H_
