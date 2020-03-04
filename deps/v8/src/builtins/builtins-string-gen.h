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
  TNode<String> GetSubstitution(TNode<Context> context,
                                TNode<String> subject_string,
                                TNode<Smi> match_start_index,
                                TNode<Smi> match_end_index,
                                TNode<String> replace_string);
  void StringEqual_Core(TNode<String> lhs, TNode<Word32T> lhs_instance_type,
                        TNode<String> rhs, TNode<Word32T> rhs_instance_type,
                        TNode<IntPtrT> length, Label* if_equal,
                        Label* if_not_equal, Label* if_indirect);
  void BranchIfStringPrimitiveWithNoCustomIteration(TNode<Object> object,
                                                    TNode<Context> context,
                                                    Label* if_true,
                                                    Label* if_false);

  TNode<Int32T> LoadSurrogatePairAt(TNode<String> string, TNode<IntPtrT> length,
                                    TNode<IntPtrT> index,
                                    UnicodeEncoding encoding);

  TNode<String> StringFromSingleUTF16EncodedCodePoint(TNode<Int32T> codepoint);

  // Return a new string object which holds a substring containing the range
  // [from,to[ of string.
  // TODO(v8:9880): Fix implementation to use UintPtrT arguments and drop
  // IntPtrT version once all callers use UintPtrT version.
  TNode<String> SubString(TNode<String> string, TNode<IntPtrT> from,
                          TNode<IntPtrT> to);
  TNode<String> SubString(TNode<String> string, TNode<UintPtrT> from,
                          TNode<UintPtrT> to) {
    return SubString(string, Signed(from), Signed(to));
  }

  // Copies |character_count| elements from |from_string| to |to_string|
  // starting at the |from_index|'th character. |from_string| and |to_string|
  // can either be one-byte strings or two-byte strings, although if
  // |from_string| is two-byte, then |to_string| must be two-byte.
  // |from_index|, |to_index| and |character_count| must be intptr_ts s.t. 0 <=
  // |from_index| <= |from_index| + |character_count| <= from_string.length and
  // 0 <= |to_index| <= |to_index| + |character_count| <= to_string.length.
  template <typename T>
  void CopyStringCharacters(TNode<T> from_string, TNode<String> to_string,
                            TNode<IntPtrT> from_index, TNode<IntPtrT> to_index,
                            TNode<IntPtrT> character_count,
                            String::Encoding from_encoding,
                            String::Encoding to_encoding);

 protected:
  void StringEqual_Loop(TNode<String> lhs, TNode<Word32T> lhs_instance_type,
                        MachineType lhs_type, TNode<String> rhs,
                        TNode<Word32T> rhs_instance_type, MachineType rhs_type,
                        TNode<IntPtrT> length, Label* if_equal,
                        Label* if_not_equal);
  TNode<IntPtrT> DirectStringData(TNode<String> string,
                                  TNode<Word32T> string_instance_type);

  void DispatchOnStringEncodings(const TNode<Word32T> lhs_instance_type,
                                 const TNode<Word32T> rhs_instance_type,
                                 Label* if_one_one, Label* if_one_two,
                                 Label* if_two_one, Label* if_two_two);

  template <typename SubjectChar, typename PatternChar>
  TNode<IntPtrT> CallSearchStringRaw(const TNode<RawPtrT> subject_ptr,
                                     const TNode<IntPtrT> subject_length,
                                     const TNode<RawPtrT> search_ptr,
                                     const TNode<IntPtrT> search_length,
                                     const TNode<IntPtrT> start_position);

  TNode<RawPtrT> PointerToStringDataAtIndex(TNode<RawPtrT> string_data,
                                            TNode<IntPtrT> index,
                                            String::Encoding encoding);

  void GenerateStringEqual(TNode<String> left, TNode<String> right);
  void GenerateStringRelationalComparison(TNode<String> left,
                                          TNode<String> right, Operation op);

  using StringAtAccessor = std::function<TNode<Object>(
      TNode<String> receiver, TNode<IntPtrT> length, TNode<IntPtrT> index)>;

  void StringIndexOf(const TNode<String> subject_string,
                     const TNode<String> search_string,
                     const TNode<Smi> position,
                     const std::function<void(TNode<Smi>)>& f_return);

  const TNode<Smi> IndexOfDollarChar(const TNode<Context> context,
                                     const TNode<String> string);

  TNode<JSArray> StringToArray(TNode<NativeContext> context,
                               TNode<String> subject_string,
                               TNode<Smi> subject_length,
                               TNode<Number> limit_number);

  TNode<BoolT> SmiIsNegative(TNode<Smi> value) {
    return SmiLessThan(value, SmiConstant(0));
  }

  TNode<String> AllocateConsString(TNode<Uint32T> length, TNode<String> left,
                                   TNode<String> right);

  TNode<String> StringAdd(SloppyTNode<Context> context, TNode<String> left,
                          TNode<String> right);

  // Check if |string| is an indirect (thin or flat cons) string type that can
  // be dereferenced by DerefIndirectString.
  void BranchIfCanDerefIndirectString(TNode<String> string,
                                      TNode<Int32T> instance_type,
                                      Label* can_deref, Label* cannot_deref);
  // Allocate an appropriate one- or two-byte ConsString with the first and
  // second parts specified by |left| and |right|.
  // Unpack an indirect (thin or flat cons) string type.
  void DerefIndirectString(TVariable<String>* var_string,
                           TNode<Int32T> instance_type);
  // Check if |var_string| has an indirect (thin or flat cons) string type, and
  // unpack it if so.
  void MaybeDerefIndirectString(TVariable<String>* var_string,
                                TNode<Int32T> instance_type, Label* did_deref,
                                Label* cannot_deref);
  // Check if |var_left| or |var_right| has an indirect (thin or flat cons)
  // string type, and unpack it/them if so. Fall through if nothing was done.
  void MaybeDerefIndirectStrings(TVariable<String>* var_left,
                                 TNode<Int32T> left_instance_type,
                                 TVariable<String>* var_right,
                                 TNode<Int32T> right_instance_type,
                                 Label* did_something);
  TNode<String> DerefIndirectString(TNode<String> string,
                                    TNode<Int32T> instance_type,
                                    Label* cannot_deref);

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
  using NodeFunction1 = std::function<void(TNode<Object> fn)>;
  using DescriptorIndexNameValue =
      PrototypeCheckAssembler::DescriptorIndexNameValue;
  void MaybeCallFunctionAtSymbol(
      const TNode<Context> context, const TNode<Object> object,
      const TNode<Object> maybe_string, Handle<Symbol> symbol,
      DescriptorIndexNameValue additional_property_to_check,
      const NodeFunction0& regexp_call, const NodeFunction1& generic_call);

 private:
  template <typename T>
  TNode<String> AllocAndCopyStringCharacters(TNode<T> from,
                                             TNode<Int32T> from_instance_type,
                                             TNode<IntPtrT> from_index,
                                             TNode<IntPtrT> character_count);
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
      const TNode<Word32T> char_code, Label* const if_not_whitespace);

 protected:
  void Generate(String::TrimMode mode, const char* method, TNode<IntPtrT> argc,
                TNode<Context> context);

  void ScanForNonWhiteSpaceOrLineTerminator(
      const TNode<RawPtrT> string_data, const TNode<IntPtrT> string_data_offset,
      const TNode<BoolT> is_stringonebyte, TVariable<IntPtrT>* const var_index,
      const TNode<IntPtrT> end, int increment, Label* const if_none_found);

  template <typename T>
  void BuildLoop(
      TVariable<IntPtrT>* const var_index, const TNode<IntPtrT> end,
      int increment, Label* const if_none_found, Label* const out,
      const std::function<TNode<T>(const TNode<IntPtrT>)>& get_character);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_STRING_GEN_H_
