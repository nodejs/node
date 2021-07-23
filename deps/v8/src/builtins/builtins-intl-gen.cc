// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-list-format.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

class IntlBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit IntlBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void ListFormatCommon(TNode<Context> context, TNode<Int32T> argc,
                        Runtime::FunctionId format_func_id,
                        const char* method_name);

  TNode<JSArray> AllocateEmptyJSArray(TNode<Context> context);

  TNode<IntPtrT> PointerToSeqStringData(TNode<String> seq_string) {
    CSA_ASSERT(this,
               IsSequentialStringInstanceType(LoadInstanceType(seq_string)));
    STATIC_ASSERT(SeqOneByteString::kHeaderSize ==
                  SeqTwoByteString::kHeaderSize);
    return IntPtrAdd(
        BitcastTaggedToWord(seq_string),
        IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  }
};

TF_BUILTIN(StringToLowerCaseIntl, IntlBuiltinsAssembler) {
  const auto string = Parameter<String>(Descriptor::kString);

  Label call_c(this), return_string(this), runtime(this, Label::kDeferred);

  // Early exit on empty strings.
  const TNode<Uint32T> length = LoadStringLengthAsWord32(string);
  GotoIf(Word32Equal(length, Uint32Constant(0)), &return_string);

  // Unpack strings if possible, and bail to runtime unless we get a one-byte
  // flat string.
  ToDirectStringAssembler to_direct(
      state(), string, ToDirectStringAssembler::kDontUnpackSlicedStrings);
  to_direct.TryToDirect(&runtime);

  const TNode<Int32T> instance_type = to_direct.instance_type();
  CSA_ASSERT(this,
             Word32BinaryNot(IsIndirectStringInstanceType(instance_type)));
  GotoIfNot(IsOneByteStringInstanceType(instance_type), &runtime);

  // For short strings, do the conversion in CSA through the lookup table.

  const TNode<String> dst = AllocateSeqOneByteString(length);

  const int kMaxShortStringLength = 24;  // Determined empirically.
  GotoIf(Uint32GreaterThan(length, Uint32Constant(kMaxShortStringLength)),
         &call_c);

  {
    const TNode<IntPtrT> dst_ptr = PointerToSeqStringData(dst);
    TVARIABLE(IntPtrT, var_cursor, IntPtrConstant(0));

    const TNode<IntPtrT> start_address =
        ReinterpretCast<IntPtrT>(to_direct.PointerToData(&call_c));
    const TNode<IntPtrT> end_address =
        Signed(IntPtrAdd(start_address, ChangeUint32ToWord(length)));

    const TNode<ExternalReference> to_lower_table_addr =
        ExternalConstant(ExternalReference::intl_to_latin1_lower_table());

    TVARIABLE(Word32T, var_did_change, Int32Constant(0));

    VariableList push_vars({&var_cursor, &var_did_change}, zone());
    BuildFastLoop<IntPtrT>(
        push_vars, start_address, end_address,
        [&](TNode<IntPtrT> current) {
          TNode<Uint8T> c = Load<Uint8T>(current);
          TNode<Uint8T> lower =
              Load<Uint8T>(to_lower_table_addr, ChangeInt32ToIntPtr(c));
          StoreNoWriteBarrier(MachineRepresentation::kWord8, dst_ptr,
                              var_cursor.value(), lower);

          var_did_change =
              Word32Or(Word32NotEqual(c, lower), var_did_change.value());

          Increment(&var_cursor);
        },
        kCharSize, IndexAdvanceMode::kPost);

    // Return the original string if it remained unchanged in order to preserve
    // e.g. internalization and private symbols (such as the preserved object
    // hash) on the source string.
    GotoIfNot(var_did_change.value(), &return_string);

    Return(dst);
  }

  // Call into C for case conversion. The signature is:
  // String ConvertOneByteToLower(String src, String dst);
  BIND(&call_c);
  {
    const TNode<String> src = to_direct.string();

    const TNode<ExternalReference> function_addr =
        ExternalConstant(ExternalReference::intl_convert_one_byte_to_lower());

    MachineType type_tagged = MachineType::AnyTagged();

    const TNode<String> result = CAST(CallCFunction(
        function_addr, type_tagged, std::make_pair(type_tagged, src),
        std::make_pair(type_tagged, dst)));

    Return(result);
  }

  BIND(&return_string);
  Return(string);

  BIND(&runtime);
  {
    const TNode<Object> result = CallRuntime(Runtime::kStringToLowerCaseIntl,
                                             NoContextConstant(), string);
    Return(result);
  }
}

TF_BUILTIN(StringPrototypeToLowerCaseIntl, IntlBuiltinsAssembler) {
  auto maybe_string = Parameter<Object>(Descriptor::kReceiver);
  auto context = Parameter<Context>(Descriptor::kContext);

  TNode<String> string =
      ToThisString(context, maybe_string, "String.prototype.toLowerCase");

  Return(CallBuiltin(Builtin::kStringToLowerCaseIntl, context, string));
}

void IntlBuiltinsAssembler::ListFormatCommon(TNode<Context> context,
                                             TNode<Int32T> argc,
                                             Runtime::FunctionId format_func_id,
                                             const char* method_name) {
  CodeStubArguments args(this, argc);

  // Label has_list(this);
  // 1. Let lf be this value.
  // 2. If Type(lf) is not Object, throw a TypeError exception.
  TNode<Object> receiver = args.GetReceiver();

  // 3. If lf does not have an [[InitializedListFormat]] internal slot, throw a
  // TypeError exception.
  ThrowIfNotInstanceType(context, receiver, JS_LIST_FORMAT_TYPE, method_name);
  TNode<JSListFormat> list_format = CAST(receiver);

  TNode<Object> list = args.GetOptionalArgumentValue(0);
  {
    // 4. Let stringList be ? StringListFromIterable(list).
    TNode<Object> string_list =
        CallBuiltin(Builtin::kStringListFromIterable, context, list);

    // 6. Return ? FormatList(lf, stringList).
    args.PopAndReturn(
        CallRuntime(format_func_id, context, list_format, string_list));
  }
}

TNode<JSArray> IntlBuiltinsAssembler::AllocateEmptyJSArray(
    TNode<Context> context) {
  return CodeStubAssembler::AllocateJSArray(
      PACKED_ELEMENTS,
      LoadJSArrayElementsMap(PACKED_ELEMENTS, LoadNativeContext(context)),
      IntPtrConstant(0), SmiConstant(0));
}

TF_BUILTIN(ListFormatPrototypeFormat, IntlBuiltinsAssembler) {
  ListFormatCommon(
      Parameter<Context>(Descriptor::kContext),
      UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount),
      Runtime::kFormatList, "Intl.ListFormat.prototype.format");
}

TF_BUILTIN(ListFormatPrototypeFormatToParts, IntlBuiltinsAssembler) {
  ListFormatCommon(
      Parameter<Context>(Descriptor::kContext),
      UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount),
      Runtime::kFormatListToParts, "Intl.ListFormat.prototype.formatToParts");
}

}  // namespace internal
}  // namespace v8
