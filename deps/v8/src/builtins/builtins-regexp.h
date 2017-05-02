// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_REGEXP_H_
#define V8_BUILTINS_BUILTINS_REGEXP_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;
typedef compiler::CodeAssemblerState CodeAssemblerState;
typedef compiler::CodeAssemblerLabel CodeAssemblerLabel;

class RegExpBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit RegExpBuiltinsAssembler(CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void BranchIfFastRegExp(Node* const context, Node* const object,
                          Node* const map, Label* const if_isunmodified,
                          Label* const if_ismodified);

 protected:
  Node* FastLoadLastIndex(Node* regexp);
  Node* SlowLoadLastIndex(Node* context, Node* regexp);
  Node* LoadLastIndex(Node* context, Node* regexp, bool is_fastpath);

  void FastStoreLastIndex(Node* regexp, Node* value);
  void SlowStoreLastIndex(Node* context, Node* regexp, Node* value);
  void StoreLastIndex(Node* context, Node* regexp, Node* value,
                      bool is_fastpath);

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

  // Analogous to BranchIfFastRegExp, for use in asserts.
  Node* IsFastRegExpMap(Node* const context, Node* const object,
                        Node* const map);

  Node* IsInitialRegExpMap(Node* context, Node* object, Node* map);
  void BranchIfFastRegExpResult(Node* context, Node* map,
                                Label* if_isunmodified, Label* if_ismodified);

  Node* FlagsGetter(Node* const context, Node* const regexp, bool is_fastpath);

  Node* FastFlagGetter(Node* const regexp, JSRegExp::Flag flag);
  Node* SlowFlagGetter(Node* const context, Node* const regexp,
                       JSRegExp::Flag flag);
  Node* FlagGetter(Node* const context, Node* const regexp, JSRegExp::Flag flag,
                   bool is_fastpath);
  void FlagGetter(JSRegExp::Flag flag, v8::Isolate::UseCounterFeature counter,
                  const char* method_name);

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
