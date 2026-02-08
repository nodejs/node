// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-string-tsa-inl.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/turboshaft-builtins-assembler-inl.h"
#include "src/common/globals.h"
#include "src/compiler/globals.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/string-view.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/objects/string.h"
#include "src/objects/tagged-field.h"

namespace v8::internal {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

using namespace compiler::turboshaft;  // NOLINT(build/namespaces)

#ifdef V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS

TS_BUILTIN(StringFromCodePointAt, StringBuiltinsAssemblerTS) {
  auto receiver = Parameter<String>(Descriptor::kReceiver);
  auto position = Parameter<WordPtr>(Descriptor::kPosition);

  // Load the character code at the {position} from the {receiver}.
  V<Word32> codepoint =
      LoadSurrogatePairAt(receiver, {}, position, UnicodeEncoding::UTF16);
  // Create a String from the UTF16 encoded code point
  V<String> result =
      StringFromSingleCodePoint(codepoint, UnicodeEncoding::UTF16);
  Return(result);
}

// ES6 #sec-string.fromcharcode
TS_BUILTIN(StringFromCharCode, StringBuiltinsAssemblerTS) {
  V<Context> context = Parameter<Context>(Descriptor::kContext);
  V<Word32> argc = Parameter<Word32>(Descriptor::kJSActualArgumentsCount);
  BuiltinArgumentsTS arguments(this, argc);

  V<WordPtr> character_count = arguments.GetLengthWithoutReceiver();
  // Check if we have exactly one argument (plus the implicit receiver), i.e.
  // if the parent frame is not an inlined arguments frame.
  IF (WordPtrEqual(arguments.GetLengthWithoutReceiver(), 1)) {
    // Single argument case, perform fast single character string cache lookup
    // for one-byte code units, or fall back to creating a single character
    // string on the fly otherwise.
    V<Object> code = arguments.AtIndex(0);
    V<Word32> code32 = TruncateTaggedToWord32(context, code);
    V<Word32> code16 = Word32BitwiseAnd(code32, String::kMaxUtf16CodeUnit);
    V<String> result = StringFromSingleCharCode(code16);
    PopAndReturn(arguments, result);
  } ELSE {
    Label<> contains_two_byte_characters(this);

    // Assume that the resulting string contains only one-byte characters.
    V<SeqOneByteString> one_byte_result =
        AllocateSeqOneByteString(character_count);

    ScopedVar<WordPtr> var_max_index(this, 0);
    // Iterate over the incoming arguments, converting them to 8-bit character
    // codes. Stop if any of the conversions generates a code that doesn't fit
    // in 8 bits.
    FOREACH(arg, arguments.Range()) {
      V<Word32> code32 = TruncateTaggedToWord32(context, arg);
      V<Word32> code16 = Word32BitwiseAnd(code32, String::kMaxUtf16CodeUnit);

      IF (UNLIKELY(Int32LessThan(String::kMaxOneByteCharCode, code16))) {
        // At least one of the characters in the string requires a 16-bit
        // representation.  Allocate a SeqTwoByteString to hold the resulting
        // string.
        V<SeqTwoByteString> two_byte_result =
            AllocateSeqTwoByteString(character_count);

        // Copy the characters that have already been put in the 8-bit string
        // into their corresponding positions in the new 16-bit string.
        CopyStringCharacters(one_byte_result, 0, String::ONE_BYTE_ENCODING,
                             two_byte_result, 0, String::TWO_BYTE_ENCODING,
                             var_max_index);

        // Write the character that caused the 8-bit to 16-bit fault.
        StoreElement(two_byte_result,
                     AccessBuilderTS::ForSeqTwoByteStringCharacter(),
                     var_max_index, code16);
        var_max_index = WordPtrAdd(var_max_index, 1);

        // Resume copying the passed-in arguments from the same place where the
        // 8-bit copy stopped, but this time copying over all of the characters
        // using a 16-bit representation.
        FOREACH(rem_arg, arguments.Range(var_max_index)) {
          V<Word32> rem_code32 = TruncateTaggedToWord32(context, rem_arg);
          V<Word32> rem_code16 =
              Word32BitwiseAnd(rem_code32, String::kMaxUtf16CodeUnit);

          StoreElement(two_byte_result,
                       AccessBuilderTS::ForSeqTwoByteStringCharacter(),
                       var_max_index, rem_code16);
          var_max_index = WordPtrAdd(var_max_index, 1);
        }
        PopAndReturn(arguments, two_byte_result);
      }

      // The {code16} fits into the SeqOneByteString {one_byte_result}.
      StoreElement(one_byte_result,
                   AccessBuilderTS::ForSeqOneByteStringCharacter(),
                   var_max_index, code16);
      var_max_index = WordPtrAdd(var_max_index, 1);
    }
    PopAndReturn(arguments, one_byte_result);
  }
}

TS_BUILTIN(ToString, StringBuiltinsAssemblerTS) {
  V<Context> context = Parameter<Context>(Descriptor::kContext);
  V<JSAny> o = Parameter<JSAny>(Descriptor::kO);
  Return(ToStringImpl(context, o));
}

#endif  // V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal
