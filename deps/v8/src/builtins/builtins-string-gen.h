// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_STRING_GEN_H_
#define V8_BUILTINS_BUILTINS_STRING_GEN_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

class StringBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit StringBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  // ES#sec-getsubstitution
  Node* GetSubstitution(Node* context, Node* subject_string,
                        Node* match_start_index, Node* match_end_index,
                        Node* replace_string);
  void StringEqual_Core(SloppyTNode<String> lhs, Node* lhs_instance_type,
                        SloppyTNode<String> rhs, Node* rhs_instance_type,
                        TNode<IntPtrT> length, Label* if_equal,
                        Label* if_not_equal, Label* if_indirect);
  void BranchIfStringPrimitiveWithNoCustomIteration(TNode<Object> object,
                                                    TNode<Context> context,
                                                    Label* if_true,
                                                    Label* if_false);

  TNode<Int32T> LoadSurrogatePairAt(SloppyTNode<String> string,
                                    SloppyTNode<IntPtrT> length,
                                    SloppyTNode<IntPtrT> index,
                                    UnicodeEncoding encoding);

 protected:
  void StringEqual_Loop(Node* lhs, Node* lhs_instance_type,
                        MachineType lhs_type, Node* rhs,
                        Node* rhs_instance_type, MachineType rhs_type,
                        TNode<IntPtrT> length, Label* if_equal,
                        Label* if_not_equal);
  Node* DirectStringData(Node* string, Node* string_instance_type);

  void DispatchOnStringEncodings(Node* const lhs_instance_type,
                                 Node* const rhs_instance_type,
                                 Label* if_one_one, Label* if_one_two,
                                 Label* if_two_one, Label* if_two_two);

  template <typename SubjectChar, typename PatternChar>
  Node* CallSearchStringRaw(Node* const subject_ptr, Node* const subject_length,
                            Node* const search_ptr, Node* const search_length,
                            Node* const start_position);

  TNode<IntPtrT> PointerToStringDataAtIndex(Node* const string_data,
                                            Node* const index,
                                            String::Encoding encoding);

  // substr and slice have a common way of handling the {start} argument.
  void ConvertAndBoundsCheckStartArgument(Node* context, Variable* var_start,
                                          Node* start, Node* string_length);

  void GenerateStringEqual(TNode<String> left, TNode<String> right);
  void GenerateStringRelationalComparison(TNode<String> left,
                                          TNode<String> right, Operation op);

  using StringAtAccessor = std::function<TNode<Object>(
      TNode<String> receiver, TNode<IntPtrT> length, TNode<IntPtrT> index)>;

  void StringIndexOf(TNode<String> const subject_string,
                     TNode<String> const search_string,
                     TNode<Smi> const position,
                     const std::function<void(TNode<Smi>)>& f_return);

  TNode<Smi> IndexOfDollarChar(Node* const context, Node* const string);

  TNode<JSArray> StringToArray(TNode<NativeContext> context,
                               TNode<String> subject_string,
                               TNode<Smi> subject_length,
                               TNode<Number> limit_number);

  TNode<BoolT> SmiIsNegative(TNode<Smi> value) {
    return SmiLessThan(value, SmiConstant(0));
  }

  // Implements boilerplate logic for {match, split, replace, search} of the
  // form:
  //
  //  if (!IS_NULL_OR_UNDEFINED(object)) {
  //    var maybe_function = object[symbol];
  //    if (!IS_UNDEFINED(maybe_function)) {
  //      return %_Call(maybe_function, ...);
  //    }
  //  }
  //
  // Contains fast paths for Smi and RegExp objects.
  // Important: {regexp_call} may not contain any code that can call into JS.
  using NodeFunction0 = std::function<void()>;
  using NodeFunction1 = std::function<void(Node* fn)>;
  using DescriptorIndexNameValue =
      PrototypeCheckAssembler::DescriptorIndexNameValue;
  void MaybeCallFunctionAtSymbol(
      Node* const context, Node* const object, Node* const maybe_string,
      Handle<Symbol> symbol,
      DescriptorIndexNameValue additional_property_to_check,
      const NodeFunction0& regexp_call, const NodeFunction1& generic_call);
};

class StringIncludesIndexOfAssembler : public StringBuiltinsAssembler {
 public:
  explicit StringIncludesIndexOfAssembler(compiler::CodeAssemblerState* state)
      : StringBuiltinsAssembler(state) {}

 protected:
  enum SearchVariant { kIncludes, kIndexOf };

  void Generate(SearchVariant variant, TNode<IntPtrT> argc,
                TNode<Context> context);
};

class StringTrimAssembler : public StringBuiltinsAssembler {
 public:
  explicit StringTrimAssembler(compiler::CodeAssemblerState* state)
      : StringBuiltinsAssembler(state) {}

  V8_EXPORT_PRIVATE void GotoIfNotWhiteSpaceOrLineTerminator(
      TNode<Word32T> const char_code, Label* const if_not_whitespace);

 protected:
  void Generate(String::TrimMode mode, const char* method, TNode<IntPtrT> argc,
                TNode<Context> context);

  void ScanForNonWhiteSpaceOrLineTerminator(
      Node* const string_data, Node* const string_data_offset,
      Node* const is_stringonebyte, TVariable<IntPtrT>* const var_index,
      TNode<IntPtrT> const end, int increment, Label* const if_none_found);

  void BuildLoop(TVariable<IntPtrT>* const var_index, TNode<IntPtrT> const end,
                 int increment, Label* const if_none_found, Label* const out,
                 const std::function<Node*(Node*)>& get_character);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_STRING_GEN_H_
