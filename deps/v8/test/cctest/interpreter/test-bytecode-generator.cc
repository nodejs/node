// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/compiler.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-generator.h"
#include "src/interpreter/interpreter.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-feedback-vector.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeGeneratorHelper {
 public:
  const char* kFunctionName = "f";

  static const int kLastParamIndex =
      -InterpreterFrameConstants::kLastParamFromRegisterPointer / kPointerSize;

  BytecodeGeneratorHelper() {
    i::FLAG_ignition = true;
    i::FLAG_ignition_fake_try_catch = true;
    i::FLAG_ignition_fallback_on_eval_and_catch = false;
    i::FLAG_ignition_filter = StrDup(kFunctionName);
    i::FLAG_always_opt = false;
    i::FLAG_allow_natives_syntax = true;
    i::FLAG_legacy_const = true;
    CcTest::i_isolate()->interpreter()->Initialize();
  }

  Isolate* isolate() { return CcTest::i_isolate(); }
  Factory* factory() { return CcTest::i_isolate()->factory(); }

  Handle<BytecodeArray> MakeTopLevelBytecode(const char* source) {
    const char* old_ignition_filter = i::FLAG_ignition_filter;
    i::FLAG_ignition_filter = "*";
    Local<v8::Script> script = v8_compile(source);
    i::FLAG_ignition_filter = old_ignition_filter;
    i::Handle<i::JSFunction> js_function = v8::Utils::OpenHandle(*script);
    return handle(js_function->shared()->bytecode_array(), CcTest::i_isolate());
  }

  Handle<BytecodeArray> MakeBytecode(const char* script,
                                     const char* function_name) {
    CompileRun(script);
    v8::Local<v8::Context> context =
        v8::Isolate::GetCurrent()->GetCurrentContext();
    Local<Function> function = Local<Function>::Cast(
        CcTest::global()->Get(context, v8_str(function_name)).ToLocalChecked());
    i::Handle<i::JSFunction> js_function =
        i::Handle<i::JSFunction>::cast(v8::Utils::OpenHandle(*function));
    return handle(js_function->shared()->bytecode_array(), CcTest::i_isolate());
  }

  Handle<BytecodeArray> MakeBytecode(const char* script, const char* filter,
                                     const char* function_name) {
    const char* old_ignition_filter = i::FLAG_ignition_filter;
    i::FLAG_ignition_filter = filter;
    Handle<BytecodeArray> return_val = MakeBytecode(script, function_name);
    i::FLAG_ignition_filter = old_ignition_filter;
    return return_val;
  }

  Handle<BytecodeArray> MakeBytecodeForFunctionBody(const char* body) {
    static const char kFormat[] = "function %s() { %s }\n%s();";
    static const int kFormatLength = arraysize(kFormat);
    int length = kFormatLength + 2 * StrLength(kFunctionName) + StrLength(body);
    ScopedVector<char> program(length);
    length = SNPrintF(program, kFormat, kFunctionName, body, kFunctionName);
    CHECK_GT(length, 0);
    return MakeBytecode(program.start(), kFunctionName);
  }

  Handle<BytecodeArray> MakeBytecodeForFunction(const char* function) {
    ScopedVector<char> program(3072);
    SNPrintF(program, "%s\n%s();", function, kFunctionName);
    return MakeBytecode(program.start(), kFunctionName);
  }

  Handle<BytecodeArray> MakeBytecodeForFunctionNoFilter(const char* function) {
    ScopedVector<char> program(3072);
    SNPrintF(program, "%s\n%s();", function, kFunctionName);
    return MakeBytecode(program.start(), "*", kFunctionName);
  }
};


// Helper macros for handcrafting bytecode sequences.
#define B(x) static_cast<uint8_t>(Bytecode::k##x)
#define U8(x) static_cast<uint8_t>((x) & 0xff)
#define R(x) static_cast<uint8_t>(-(x) & 0xff)
#define A(x, n) R(helper.kLastParamIndex - (n) + 1 + (x))
#define THIS(n) A(0, n)
#if defined(V8_TARGET_LITTLE_ENDIAN)
#define U16(x) static_cast<uint8_t>((x) & 0xff),                    \
               static_cast<uint8_t>(((x) >> kBitsPerByte) & 0xff)
#define U16I(x) static_cast<uint8_t>((x) & 0xff),                   \
                static_cast<uint8_t>(((x++) >> kBitsPerByte) & 0xff)
#elif defined(V8_TARGET_BIG_ENDIAN)
#define U16(x) static_cast<uint8_t>(((x) >> kBitsPerByte) & 0xff),   \
               static_cast<uint8_t>((x) & 0xff)
#define U16I(x) static_cast<uint8_t>(((x) >> kBitsPerByte) & 0xff),  \
                static_cast<uint8_t>((x++) & 0xff)
#else
#error Unknown byte ordering
#endif

#define XSTR(A) #A
#define STR(A) XSTR(A)

#define COMMA() ,
#define SPACE()
#define UNIQUE_VAR() "var a" STR(__COUNTER__) " = 0;\n"

#define REPEAT_2(SEP, ...)                      \
  __VA_ARGS__ SEP() __VA_ARGS__
#define REPEAT_4(SEP, ...)  \
  REPEAT_2(SEP, __VA_ARGS__) SEP() REPEAT_2(SEP, __VA_ARGS__)
#define REPEAT_8(SEP, ...)  \
  REPEAT_4(SEP, __VA_ARGS__) SEP() REPEAT_4(SEP, __VA_ARGS__)
#define REPEAT_16(SEP, ...)  \
  REPEAT_8(SEP, __VA_ARGS__) SEP() REPEAT_8(SEP, __VA_ARGS__)
#define REPEAT_32(SEP, ...)  \
  REPEAT_16(SEP, __VA_ARGS__) SEP() REPEAT_16(SEP, __VA_ARGS__)
#define REPEAT_64(SEP, ...)  \
  REPEAT_32(SEP, __VA_ARGS__) SEP() REPEAT_32(SEP, __VA_ARGS__)
#define REPEAT_128(SEP, ...)  \
  REPEAT_64(SEP, __VA_ARGS__) SEP() REPEAT_64(SEP, __VA_ARGS__)
#define REPEAT_256(SEP, ...)  \
  REPEAT_128(SEP, __VA_ARGS__) SEP() REPEAT_128(SEP, __VA_ARGS__)

#define REPEAT_127(SEP, ...)                                           \
  REPEAT_64(SEP, __VA_ARGS__) SEP() REPEAT_32(SEP, __VA_ARGS__) SEP()  \
  REPEAT_16(SEP, __VA_ARGS__) SEP() REPEAT_8(SEP, __VA_ARGS__) SEP()   \
  REPEAT_4(SEP, __VA_ARGS__) SEP() REPEAT_2(SEP, __VA_ARGS__) SEP()    \
  __VA_ARGS__

#define REPEAT_249(SEP, ...)                                            \
  REPEAT_127(SEP, __VA_ARGS__) SEP() REPEAT_64(SEP, __VA_ARGS__) SEP()  \
  REPEAT_32(SEP, __VA_ARGS__) SEP() REPEAT_16(SEP, __VA_ARGS__) SEP()   \
  REPEAT_8(SEP, __VA_ARGS__) SEP() REPEAT_2(SEP, __VA_ARGS__)

#define REPEAT_249_UNIQUE_VARS()                                        \
UNIQUE_VAR() REPEAT_127(UNIQUE_VAR) UNIQUE_VAR() REPEAT_64(UNIQUE_VAR)  \
UNIQUE_VAR() REPEAT_32(UNIQUE_VAR) UNIQUE_VAR() REPEAT_16(UNIQUE_VAR)   \
UNIQUE_VAR() REPEAT_8(UNIQUE_VAR) UNIQUE_VAR() REPEAT_2(UNIQUE_VAR)

// Structure for containing expected bytecode snippets.
template<typename T, int C = 6>
struct ExpectedSnippet {
  const char* code_snippet;
  int frame_size;
  int parameter_count;
  int bytecode_length;
  const uint8_t bytecode[2048];
  int constant_count;
  T constants[C];
};


static void CheckConstant(int expected, Object* actual) {
  CHECK_EQ(expected, Smi::cast(actual)->value());
}


static void CheckConstant(double expected, Object* actual) {
  CHECK_EQ(expected, HeapNumber::cast(actual)->value());
}


static void CheckConstant(const char* expected, Object* actual) {
  Handle<String> expected_string =
      CcTest::i_isolate()->factory()->NewStringFromAsciiChecked(expected);
  CHECK(String::cast(actual)->Equals(*expected_string));
}


static void CheckConstant(Handle<Object> expected, Object* actual) {
  CHECK(actual == *expected || expected->StrictEquals(actual));
}


static void CheckConstant(InstanceType expected, Object* actual) {
  CHECK_EQ(expected, HeapObject::cast(actual)->map()->instance_type());
}


template <typename T, int C>
static void CheckBytecodeArrayEqual(const ExpectedSnippet<T, C>& expected,
                                    Handle<BytecodeArray> actual) {
  CHECK_EQ(expected.frame_size, actual->frame_size());
  CHECK_EQ(expected.parameter_count, actual->parameter_count());
  CHECK_EQ(expected.bytecode_length, actual->length());
  if (expected.constant_count == 0) {
    CHECK_EQ(CcTest::heap()->empty_fixed_array(), actual->constant_pool());
  } else {
    CHECK_EQ(expected.constant_count, actual->constant_pool()->length());
    for (int i = 0; i < expected.constant_count; i++) {
      CheckConstant(expected.constants[i], actual->constant_pool()->get(i));
    }
  }

  BytecodeArrayIterator iterator(actual);
  int i = 0;
  while (!iterator.done()) {
    int bytecode_index = i++;
    Bytecode bytecode = iterator.current_bytecode();
    if (Bytecodes::ToByte(bytecode) != expected.bytecode[bytecode_index]) {
      std::ostringstream stream;
      stream << "Check failed: expected bytecode [" << bytecode_index
             << "] to be " << Bytecodes::ToString(static_cast<Bytecode>(
                                  expected.bytecode[bytecode_index]))
             << " but got " << Bytecodes::ToString(bytecode);
      FATAL(stream.str().c_str());
    }
    for (int j = 0; j < Bytecodes::NumberOfOperands(bytecode); ++j) {
      OperandType operand_type = Bytecodes::GetOperandType(bytecode, j);
      int operand_index = i;
      i += static_cast<int>(Bytecodes::SizeOfOperand(operand_type));
      uint32_t raw_operand = iterator.GetRawOperand(j, operand_type);
      uint32_t expected_operand;
      switch (Bytecodes::SizeOfOperand(operand_type)) {
        case OperandSize::kNone:
          UNREACHABLE();
          return;
        case OperandSize::kByte:
          expected_operand =
              static_cast<uint32_t>(expected.bytecode[operand_index]);
          break;
        case OperandSize::kShort:
          expected_operand =
              ReadUnalignedUInt16(&expected.bytecode[operand_index]);
          break;
        default:
          UNREACHABLE();
          return;
      }
      if (raw_operand != expected_operand) {
        std::ostringstream stream;
        stream << "Check failed: expected operand [" << j << "] for bytecode ["
               << bytecode_index << "] to be "
               << static_cast<unsigned int>(expected_operand) << " but got "
               << static_cast<unsigned int>(raw_operand);
        FATAL(stream.str().c_str());
      }
    }
    iterator.Advance();
  }
}


TEST(PrimitiveReturnStatements) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"", 0, 1, 2, {B(LdaUndefined), B(Return)}, 0},
      {"return;", 0, 1, 2, {B(LdaUndefined), B(Return)}, 0},
      {"return null;", 0, 1, 2, {B(LdaNull), B(Return)}, 0},
      {"return true;", 0, 1, 2, {B(LdaTrue), B(Return)}, 0},
      {"return false;", 0, 1, 2, {B(LdaFalse), B(Return)}, 0},
      {"return 0;", 0, 1, 2, {B(LdaZero), B(Return)}, 0},
      {"return +1;", 0, 1, 3, {B(LdaSmi8), U8(1), B(Return)}, 0},
      {"return -1;", 0, 1, 3, {B(LdaSmi8), U8(-1), B(Return)}, 0},
      {"return +127;", 0, 1, 3, {B(LdaSmi8), U8(127), B(Return)}, 0},
      {"return -128;", 0, 1, 3, {B(LdaSmi8), U8(-128), B(Return)}, 0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(PrimitiveExpressions) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var x = 0; return x;",
       kPointerSize,
       1,
       4,
       {B(LdaZero),     //
        B(Star), R(0),  //
        B(Return)},
       0},
      {"var x = 0; return x + 3;",
       2 * kPointerSize,
       1,
       10,
       {B(LdaZero),         //
        B(Star), R(0),      //
        B(Star), R(1),      //
        B(LdaSmi8), U8(3),  //
        B(Add), R(1),       //
        B(Return)},
       0},
      {"var x = 0; return x - 3;",
       2 * kPointerSize,
       1,
       10,
       {B(LdaZero),         //
        B(Star), R(0),      //
        B(Star), R(1),      //
        B(LdaSmi8), U8(3),  //
        B(Sub), R(1),       //
        B(Return)},
       0},
      {"var x = 4; return x * 3;",
       2 * kPointerSize,
       1,
       11,
       {B(LdaSmi8), U8(4),  //
        B(Star), R(0),      //
        B(Star), R(1),      //
        B(LdaSmi8), U8(3),  //
        B(Mul), R(1),       //
        B(Return)},
       0},
      {"var x = 4; return x / 3;",
       2 * kPointerSize,
       1,
       11,
       {B(LdaSmi8), U8(4),  //
        B(Star), R(0),      //
        B(Star), R(1),      //
        B(LdaSmi8), U8(3),  //
        B(Div), R(1),       //
        B(Return)},
       0},
      {"var x = 4; return x % 3;",
       2 * kPointerSize,
       1,
       11,
       {B(LdaSmi8), U8(4),  //
        B(Star), R(0),      //
        B(Star), R(1),      //
        B(LdaSmi8), U8(3),  //
        B(Mod), R(1),       //
        B(Return)},
       0},
      {"var x = 1; return x | 2;",
       2 * kPointerSize,
       1,
       11,
       {B(LdaSmi8), U8(1),   //
        B(Star), R(0),       //
        B(Star), R(1),       //
        B(LdaSmi8), U8(2),   //
        B(BitwiseOr), R(1),  //
        B(Return)},
       0},
      {"var x = 1; return x ^ 2;",
       2 * kPointerSize,
       1,
       11,
       {B(LdaSmi8), U8(1),    //
        B(Star), R(0),        //
        B(Star), R(1),        //
        B(LdaSmi8), U8(2),    //
        B(BitwiseXor), R(1),  //
        B(Return)},
       0},
      {"var x = 1; return x & 2;",
       2 * kPointerSize,
       1,
       11,
       {B(LdaSmi8), U8(1),    //
        B(Star), R(0),        //
        B(Star), R(1),        //
        B(LdaSmi8), U8(2),    //
        B(BitwiseAnd), R(1),  //
        B(Return)},
       0},
      {"var x = 10; return x << 3;",
       2 * kPointerSize,
       1,
       11,
       {B(LdaSmi8), U8(10),  //
        B(Star), R(0),       //
        B(Star), R(1),       //
        B(LdaSmi8), U8(3),   //
        B(ShiftLeft), R(1),  //
        B(Return)},
       0},
      {"var x = 10; return x >> 3;",
       2 * kPointerSize,
       1,
       11,
       {B(LdaSmi8), U8(10),   //
        B(Star), R(0),        //
        B(Star), R(1),        //
        B(LdaSmi8), U8(3),    //
        B(ShiftRight), R(1),  //
        B(Return)},
       0},
      {"var x = 10; return x >>> 3;",
       2 * kPointerSize,
       1,
       11,
       {B(LdaSmi8), U8(10),          //
        B(Star), R(0),               //
        B(Star), R(1),               //
        B(LdaSmi8), U8(3),           //
        B(ShiftRightLogical), R(1),  //
        B(Return)},
       0},
      {"var x = 0; return (x, 3);",
       1 * kPointerSize,
       1,
       6,
       {B(LdaZero),         //
        B(Star), R(0),      //
        B(LdaSmi8), U8(3),  //
        B(Return)},
       0}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(LogicalExpressions) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var x = 0; return x || 3;",
       1 * kPointerSize,
       1,
       8,
       {B(LdaZero),                     //
        B(Star), R(0),                  //
        B(JumpIfToBooleanTrue), U8(4),  //
        B(LdaSmi8), U8(3),              //
        B(Return)},
       0},
      {"var x = 0; return (x == 1) || 3;",
       2 * kPointerSize,
       1,
       14,
       {B(LdaZero),            //
        B(Star), R(0),         //
        B(Star), R(1),         //
        B(LdaSmi8), U8(1),     //
        B(TestEqual), R(1),    //
        B(JumpIfTrue), U8(4),  //
        B(LdaSmi8), U8(3),     //
        B(Return)},
       0},
      {"var x = 0; return x && 3;",
       1 * kPointerSize,
       1,
       8,
       {B(LdaZero),                      //
        B(Star), R(0),                   //
        B(JumpIfToBooleanFalse), U8(4),  //
        B(LdaSmi8), U8(3),               //
        B(Return)},
       0},
      {"var x = 0; return (x == 0) && 3;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaZero),             //
        B(Star), R(0),          //
        B(Star), R(1),          //
        B(LdaZero),             //
        B(TestEqual), R(1),     //
        B(JumpIfFalse), U8(4),  //
        B(LdaSmi8), U8(3),      //
        B(Return)},
       0},
      {"var x = 0; return x || (1, 2, 3);",
       1 * kPointerSize,
       1,
       8,
       {B(LdaZero),                     //
        B(Star), R(0),                  //
        B(JumpIfToBooleanTrue), U8(4),  //
        B(LdaSmi8), U8(3),              //
        B(Return)},
       0},
      {"var a = 2, b = 3, c = 4; return a || (a, b, a, b, c = 5, 3);",
       3 * kPointerSize,
       1,
       31,
       {B(LdaSmi8), U8(2),               //
        B(Star), R(0),                   //
        B(LdaSmi8), U8(3),               //
        B(Star), R(1),                   //
        B(LdaSmi8), U8(4),               //
        B(Star), R(2),                   //
        B(Ldar), R(0),                   //
        B(JumpIfToBooleanTrue), U8(16),  //
        B(Ldar), R(0),                   //
        B(Ldar), R(1),                   //
        B(Ldar), R(0),                   //
        B(Ldar), R(1),                   //
        B(LdaSmi8), U8(5),               //
        B(Star), R(2),                   //
        B(LdaSmi8), U8(3),               //
        B(Return)},
       0},
      {"var x = 1; var a = 2, b = 3; return x || ("
       REPEAT_32(SPACE, "a = 1, b = 2, ")
       "3);",
       3 * kPointerSize,
       1,
       275,
       {B(LdaSmi8), U8(1),                      //
        B(Star), R(0),                          //
        B(LdaSmi8), U8(2),                      //
        B(Star), R(1),                          //
        B(LdaSmi8), U8(3),                      //
        B(Star), R(2),                          //
        B(Ldar), R(0),                          //
        B(JumpIfToBooleanTrueConstant), U8(0),  //
        REPEAT_32(COMMA,                        //
                  B(LdaSmi8), U8(1),            //
                  B(Star), R(1),                //
                  B(LdaSmi8), U8(2),            //
                  B(Star), R(2)),               //
        B(LdaSmi8), U8(3),                      //
        B(Return)},
       1,
       {260, 0, 0, 0}},
      {"var x = 0; var a = 2, b = 3; return x && ("
       REPEAT_32(SPACE, "a = 1, b = 2, ")
       "3);",
       3 * kPointerSize,
       1,
       274,
       {B(LdaZero),                              //
        B(Star), R(0),                           //
        B(LdaSmi8), U8(2),                       //
        B(Star), R(1),                           //
        B(LdaSmi8), U8(3),                       //
        B(Star), R(2),                           //
        B(Ldar), R(0),                           //
        B(JumpIfToBooleanFalseConstant), U8(0),  //
        REPEAT_32(COMMA,                         //
                  B(LdaSmi8), U8(1),             //
                  B(Star), R(1),                 //
                  B(LdaSmi8), U8(2),             //
                  B(Star), R(2)),                //
        B(LdaSmi8), U8(3),                       //
        B(Return)},                              //
       1,
       {260, 0, 0, 0}},
      {"var x = 1; var a = 2, b = 3; return (x > 3) || ("
        REPEAT_32(SPACE, "a = 1, b = 2, ")
       "3);",
       4 * kPointerSize,
       1,
       281,
       {B(LdaSmi8), U8(1),             //
        B(Star), R(0),                 //
        B(LdaSmi8), U8(2),             //
        B(Star), R(1),                 //
        B(LdaSmi8), U8(3),             //
        B(Star), R(2),                 //
        B(Ldar), R(0),                 //
        B(Star), R(3),                 //
        B(LdaSmi8), U8(3),             //
        B(TestGreaterThan), R(3),      //
        B(JumpIfTrueConstant), U8(0),  //
        REPEAT_32(COMMA,               //
                  B(LdaSmi8), U8(1),   //
                  B(Star), R(1),       //
                  B(LdaSmi8), U8(2),   //
                  B(Star), R(2)),      //
        B(LdaSmi8), U8(3),             //
        B(Return)},
       1,
       {260, 0, 0, 0}},
      {"var x = 0; var a = 2, b = 3; return (x < 5) && ("
        REPEAT_32(SPACE, "a = 1, b = 2, ")
       "3);",
       4 * kPointerSize,
       1,
       280,
       {B(LdaZero),                     //
        B(Star), R(0),                  //
        B(LdaSmi8), U8(2),              //
        B(Star), R(1),                  //
        B(LdaSmi8), U8(3),              //
        B(Star), R(2),                  //
        B(Ldar), R(0),                  //
        B(Star), R(3),                  //
        B(LdaSmi8), U8(5),              //
        B(TestLessThan), R(3),          //
        B(JumpIfFalseConstant), U8(0),  //
        REPEAT_32(COMMA,                //
                  B(LdaSmi8), U8(1),    //
                  B(Star), R(1),        //
                  B(LdaSmi8), U8(2),    //
                  B(Star), R(2)),       //
        B(LdaSmi8), U8(3),              //
        B(Return)},
       1,
       {260, 0, 0, 0}},
      {"return 0 && 3;",
       0 * kPointerSize,
       1,
       2,
       {B(LdaZero),  //
        B(Return)},
       0},
      {"return 1 || 3;",
       0 * kPointerSize,
       1,
       3,
       {B(LdaSmi8), U8(1),  //
        B(Return)},
       0},
      {"var x = 1; return x && 3 || 0, 1;",
       1 * kPointerSize,
       1,
       14,
       {B(LdaSmi8), U8(1),               //
        B(Star), R(0),                   //
        B(JumpIfToBooleanFalse), U8(4),  //
        B(LdaSmi8), U8(3),               //
        B(JumpIfToBooleanTrue), U8(3),   //
        B(LdaZero),                      //
        B(LdaSmi8), U8(1),               //
        B(Return)},
       0}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(Parameters) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"function f() { return this; }",
       0,
       1,
       3,
       {B(Ldar), THIS(1), B(Return)},
       0},
      {"function f(arg1) { return arg1; }",
       0,
       2,
       3,
       {B(Ldar), A(1, 2), B(Return)},
       0},
      {"function f(arg1) { return this; }",
       0,
       2,
       3,
       {B(Ldar), THIS(2), B(Return)},
       0},
      {"function f(arg1, arg2, arg3, arg4, arg5, arg6, arg7) { return arg4; }",
       0,
       8,
       3,
       {B(Ldar), A(4, 8), B(Return)},
       0},
      {"function f(arg1, arg2, arg3, arg4, arg5, arg6, arg7) { return this; }",
       0,
       8,
       3,
       {B(Ldar), THIS(8), B(Return)},
       0},
      {"function f(arg1) { arg1 = 1; }",
       0,
       2,
       6,
       {B(LdaSmi8), U8(1),  //
        B(Star), A(1, 2),   //
        B(LdaUndefined),    //
        B(Return)},
       0},
      {"function f(arg1, arg2, arg3, arg4) { arg2 = 1; }",
       0,
       5,
       6,
       {B(LdaSmi8), U8(1),  //
        B(Star), A(2, 5),   //
        B(LdaUndefined),    //
        B(Return)},
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunction(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(IntegerConstants) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
    {"return 12345678;",
     0,
     1,
     3,
     {
       B(LdaConstant), U8(0),  //
       B(Return)               //
     },
     1,
     {12345678}},
    {"var a = 1234; return 5678;",
     1 * kPointerSize,
     1,
     7,
     {
       B(LdaConstant), U8(0),  //
       B(Star), R(0),          //
       B(LdaConstant), U8(1),  //
       B(Return)               //
     },
     2,
     {1234, 5678}},
    {"var a = 1234; return 1234;",
     1 * kPointerSize,
     1,
     7,
     {
       B(LdaConstant), U8(0),  //
       B(Star), R(0),          //
       B(LdaConstant), U8(0),  //
       B(Return)               //
     },
     1,
     {1234}}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(HeapNumberConstants) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int wide_idx = 0;

  ExpectedSnippet<double, 257> snippets[] = {
    {"return 1.2;",
     0,
     1,
     3,
     {
       B(LdaConstant), U8(0),  //
       B(Return)               //
     },
     1,
     {1.2}},
    {"var a = 1.2; return 2.6;",
     1 * kPointerSize,
     1,
     7,
     {
       B(LdaConstant), U8(0),  //
       B(Star), R(0),          //
       B(LdaConstant), U8(1),  //
       B(Return)               //
     },
     2,
     {1.2, 2.6}},
    {"var a = 3.14; return 3.14;",
     1 * kPointerSize,
     1,
     7,
     {
       B(LdaConstant), U8(0),  //
       B(Star), R(0),          //
       B(LdaConstant), U8(1),  //
       B(Return)               //
     },
     2,
     {3.14, 3.14}},
    {"var a;"
     REPEAT_256(SPACE, " a = 1.414;")
     " a = 3.14;",
     1 * kPointerSize,
     1,
     1031,
     {
         REPEAT_256(COMMA,                     //
           B(LdaConstant), U8(wide_idx++),     //
           B(Star), R(0)),                     //
         B(LdaConstantWide), U16(wide_idx),    //
         B(Star), R(0),                        //
         B(LdaUndefined),                      //
         B(Return),                            //
     },
     257,
     {REPEAT_256(COMMA, 1.414),
      3.14}}
  };
  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(StringConstants) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<const char*> snippets[] = {
      {"return \"This is a string\";",
       0,
       1,
       3,
       {
           B(LdaConstant), U8(0),  //
           B(Return)               //
       },
       1,
       {"This is a string"}},
      {"var a = \"First string\"; return \"Second string\";",
       1 * kPointerSize,
       1,
       7,
       {
           B(LdaConstant), U8(0),  //
           B(Star), R(0),          //
           B(LdaConstant), U8(1),  //
           B(Return)               //
       },
       2,
       {"First string", "Second string"}},
      {"var a = \"Same string\"; return \"Same string\";",
       1 * kPointerSize,
       1,
       7,
       {
           B(LdaConstant), U8(0),  //
           B(Star), R(0),          //
           B(LdaConstant), U8(0),  //
           B(Return)               //
       },
       1,
       {"Same string"}}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(PropertyLoads) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddLoadICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  // These are a hack used by the LoadICXXXWide tests below.
  int wide_idx_1 = vector->GetIndex(slot1) - 2;
  int wide_idx_2 = vector->GetIndex(slot1) - 2;
  int wide_idx_3 = vector->GetIndex(slot1) - 2;
  int wide_idx_4 = vector->GetIndex(slot1) - 2;

  ExpectedSnippet<const char*> snippets[] = {
      {"function f(a) { return a.name; }\nf({name : \"test\"})",
       1 * kPointerSize,
       2,
       9,
       {
           B(Ldar), A(1, 2),                                           //
           B(Star), R(0),                                              //
           B(LoadICSloppy), R(0), U8(0), U8(vector->GetIndex(slot1)),  //
           B(Return),                                                  //
       },
       1,
       {"name"}},
      {"function f(a) { return a[\"key\"]; }\nf({key : \"test\"})",
       1 * kPointerSize,
       2,
       9,
       {
           B(Ldar), A(1, 2),                                           //
           B(Star), R(0),                                              //
           B(LoadICSloppy), R(0), U8(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                                   //
       },
       1,
       {"key"}},
      {"function f(a) { return a[100]; }\nf({100 : \"test\"})",
       1 * kPointerSize,
       2,
       10,
       {
           B(Ldar), A(1, 2),                                         //
           B(Star), R(0),                                            //
           B(LdaSmi8), U8(100),                                      //
           B(KeyedLoadICSloppy), R(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                                 //
       },
       0},
      {"function f(a, b) { return a[b]; }\nf({arg : \"test\"}, \"arg\")",
       1 * kPointerSize,
       3,
       10,
       {
           B(Ldar), A(1, 3),                                         //
           B(Star), R(0),                                            //
           B(Ldar), A(1, 2),                                         //
           B(KeyedLoadICSloppy), R(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                                 //
       },
       0},
      {"function f(a) { var b = a.name; return a[-124]; }\n"
       "f({\"-124\" : \"test\", name : 123 })",
       2 * kPointerSize,
       2,
       20,
       {
           B(Ldar), A(1, 2),                                           //
           B(Star), R(1),                                              //
           B(LoadICSloppy), R(1), U8(0), U8(vector->GetIndex(slot1)),  //
           B(Star), R(0),                                              //
           B(Ldar), A(1, 2),                                           //
           B(Star), R(1),                                              //
           B(LdaSmi8), U8(-124),                                       //
           B(KeyedLoadICSloppy), R(1), U8(vector->GetIndex(slot2)),    //
           B(Return),                                                  //
       },
       1,
       {"name"}},
      {"function f(a) { \"use strict\"; return a.name; }\nf({name : \"test\"})",
       1 * kPointerSize,
       2,
       9,
       {
           B(Ldar), A(1, 2),                                           //
           B(Star), R(0),                                              //
           B(LoadICStrict), R(0), U8(0), U8(vector->GetIndex(slot1)),  //
           B(Return),                                                  //
       },
       1,
       {"name"}},
      {"function f(a, b) { \"use strict\"; return a[b]; }\n"
       "f({arg : \"test\"}, \"arg\")",
       1 * kPointerSize,
       3,
       10,
       {
           B(Ldar), A(1, 3),                                         //
           B(Star), R(0),                                            //
           B(Ldar), A(2, 3),                                         //
           B(KeyedLoadICStrict), R(0), U8(vector->GetIndex(slot1)),  //
           B(Return),                                                //
       },
       0},
      {"function f(a) {\n"
       " var b;\n"
       "b = a.name;"
       REPEAT_127(SPACE, " b = a.name; ")
       " return a.name; }\n"
       "f({name : \"test\"})\n",
       2 * kPointerSize,
       2,
       1291,
       {
           B(Ldar), A(1, 2),                                        //
           B(Star), R(1),                                           //
           B(LoadICSloppy), R(1), U8(0), U8(wide_idx_1 += 2),       //
           B(Star), R(0),                                           //
           REPEAT_127(COMMA,                                        //
                      B(Ldar), A(1, 2),                             //
                      B(Star), R(1),                                //
                      B(LoadICSloppy), R(1), U8(0),                 //
                                       U8((wide_idx_1 += 2)),       //
                      B(Star), R(0)),                               //
           B(Ldar), A(1, 2),                                        //
           B(Star), R(1),                                           //
           B(LoadICSloppyWide), R(1), U16(0), U16(wide_idx_1 + 2),  //
           B(Return),                                               //
       },
       1,
       {"name"}},
      {"function f(a) {\n"
       " 'use strict'; var b;\n"
       "  b = a.name;\n"
       REPEAT_127(SPACE, " b = a.name; ")
       " return a.name; }\n"
       "f({name : \"test\"})\n",
       2 * kPointerSize,
       2,
       1291,
       {
           B(Ldar), A(1, 2),                                        //
           B(Star), R(1),                                           //
           B(LoadICStrict), R(1), U8(0), U8((wide_idx_2 += 2)),     //
           B(Star), R(0),                                           //
           REPEAT_127(COMMA,                                        //
                      B(Ldar), A(1, 2),                             //
                      B(Star), R(1),                                //
                      B(LoadICStrict), R(1), U8(0),                 //
                                       U8((wide_idx_2 += 2)),       //
                      B(Star), R(0)),                               //
           B(Ldar), A(1, 2),                                        //
           B(Star), R(1),                                           //
           B(LoadICStrictWide), R(1), U16(0), U16(wide_idx_2 + 2),  //
           B(Return),                                               //
       },
       1,
       {"name"}},
      {"function f(a, b) {\n"
       " var c;\n"
       " c = a[b];"
       REPEAT_127(SPACE, " c = a[b]; ")
       " return a[b]; }\n"
       "f({name : \"test\"}, \"name\")\n",
       2 * kPointerSize,
       3,
       1419,
       {
           B(Ldar), A(1, 3),                                              //
           B(Star), R(1),                                                 //
           B(Ldar), A(2, 3),                                              //
           B(KeyedLoadICSloppy), R(1), U8((wide_idx_3 += 2)),             //
           B(Star), R(0),                                                 //
           REPEAT_127(COMMA,                                              //
                      B(Ldar), A(1, 3),                                   //
                      B(Star), R(1),                                      //
                      B(Ldar), A(2, 3),                                   //
                      B(KeyedLoadICSloppy), R(1), U8((wide_idx_3 += 2)),  //
                      B(Star), R(0)),                                     //
           B(Ldar), A(1, 3),                                              //
           B(Star), R(1),                                                 //
           B(Ldar), A(2, 3),                                              //
           B(KeyedLoadICSloppyWide), R(1), U16(wide_idx_3 + 2),           //
           B(Return),                                                     //
       }},
      {"function f(a, b) {\n"
       " 'use strict'; var c;\n"
       "  c = a[b];"
       REPEAT_127(SPACE, " c = a[b]; ")
       " return a[b]; }\n"
       "f({name : \"test\"}, \"name\")\n",
       2 * kPointerSize,
       3,
       1419,
       {
           B(Ldar), A(1, 3),                                              //
           B(Star), R(1),                                                 //
           B(Ldar), A(2, 3),                                              //
           B(KeyedLoadICStrict), R(1), U8((wide_idx_4 += 2)),             //
           B(Star), R(0),                                                 //
           REPEAT_127(COMMA,                                              //
                      B(Ldar), A(1, 3),                                   //
                      B(Star), R(1),                                      //
                      B(Ldar), A(2, 3),                                   //
                      B(KeyedLoadICStrict), R(1), U8((wide_idx_4 += 2)),  //
                      B(Star), R(0)),                                     //
           B(Ldar), A(1, 3),                                              //
           B(Star), R(1),                                                 //
           B(Ldar), A(2, 3),                                              //
           B(KeyedLoadICStrictWide), R(1), U16(wide_idx_4 + 2),           //
           B(Return),                                                     //
       }},
  };
  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, helper.kFunctionName);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(PropertyStores) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddStoreICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  // These are a hack used by the StoreICXXXWide tests below.
  int wide_idx_1 = vector->GetIndex(slot1) - 2;
  int wide_idx_2 = vector->GetIndex(slot1) - 2;
  int wide_idx_3 = vector->GetIndex(slot1) - 2;
  int wide_idx_4 = vector->GetIndex(slot1) - 2;

  ExpectedSnippet<const char*> snippets[] = {
      {"function f(a) { a.name = \"val\"; }\nf({name : \"test\"})",
       kPointerSize,
       2,
       12,
       {
           B(Ldar), A(1, 2),                                            //
           B(Star), R(0),                                               //
           B(LdaConstant), U8(0),                                       //
           B(StoreICSloppy), R(0), U8(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                             //
           B(Return),                                                   //
       },
       2,
       {"val", "name"}},
      {"function f(a) { a[\"key\"] = \"val\"; }\nf({key : \"test\"})",
       kPointerSize,
       2,
       12,
       {
           B(Ldar), A(1, 2),                                            //
           B(Star), R(0),                                               //
           B(LdaConstant), U8(0),                                       //
           B(StoreICSloppy), R(0), U8(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                             //
           B(Return),                                                   //
       },
       2,
       {"val", "key"}},
      {"function f(a) { a[100] = \"val\"; }\nf({100 : \"test\"})",
       2 * kPointerSize,
       2,
       16,
       {
           B(Ldar), A(1, 2),                   //
           B(Star), R(0),                      //
           B(LdaSmi8), U8(100),                //
           B(Star), R(1),                      //
           B(LdaConstant), U8(0),              //
           B(KeyedStoreICSloppy), R(0), R(1),  //
           U8(vector->GetIndex(slot1)),        //
           B(LdaUndefined),                    //
           B(Return),                          //
       },
       1,
       {"val"}},
      {"function f(a, b) { a[b] = \"val\"; }\nf({arg : \"test\"}, \"arg\")",
       2 * kPointerSize,
       3,
       16,
       {
           B(Ldar), A(1, 3),                   //
           B(Star), R(0),                      //
           B(Ldar), A(2, 3),                   //
           B(Star), R(1),                      //
           B(LdaConstant), U8(0),              //
           B(KeyedStoreICSloppy), R(0), R(1),  //
           U8(vector->GetIndex(slot1)),        //
           B(LdaUndefined),                    //
           B(Return),                          //
       },
       1,
       {"val"}},
      {"function f(a) { a.name = a[-124]; }\n"
       "f({\"-124\" : \"test\", name : 123 })",
       2 * kPointerSize,
       2,
       19,
       {
           B(Ldar), A(1, 2),                                            //
           B(Star), R(0),                                               //
           B(Ldar), A(1, 2),                                            //
           B(Star), R(1),                                               //
           B(LdaSmi8), U8(-124),                                        //
           B(KeyedLoadICSloppy), R(1), U8(vector->GetIndex(slot1)),     //
           B(StoreICSloppy), R(0), U8(0), U8(vector->GetIndex(slot2)),  //
           B(LdaUndefined),                                             //
           B(Return),                                                   //
       },
       1,
       {"name"}},
      {"function f(a) { \"use strict\"; a.name = \"val\"; }\n"
       "f({name : \"test\"})",
       kPointerSize,
       2,
       12,
       {
           B(Ldar), A(1, 2),                                            //
           B(Star), R(0),                                               //
           B(LdaConstant), U8(0),                                       //
           B(StoreICStrict), R(0), U8(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                             //
           B(Return),                                                   //
       },
       2,
       {"val", "name"}},
      {"function f(a, b) { \"use strict\"; a[b] = \"val\"; }\n"
       "f({arg : \"test\"}, \"arg\")",
       2 * kPointerSize,
       3,
       16,
       {
           B(Ldar), A(1, 3),                                                //
           B(Star), R(0),                                                   //
           B(Ldar), A(2, 3),                                                //
           B(Star), R(1),                                                   //
           B(LdaConstant), U8(0),                                           //
           B(KeyedStoreICStrict), R(0), R(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                                 //
           B(Return),                                                       //
       },
       1,
       {"val"}},
      {"function f(a) {\n"
       "a.name = 1;"
       REPEAT_127(SPACE, " a.name = 1; ")
       " a.name = 2; }\n"
       "f({name : \"test\"})\n",
       kPointerSize,
       2,
       1294,
       {
           B(Ldar), A(1, 2),                                         //
           B(Star), R(0),                                            //
           B(LdaSmi8), U8(1),                                        //
           B(StoreICSloppy), R(0), U8(0), U8((wide_idx_1 += 2)),     //
           REPEAT_127(COMMA,                                         //
                      B(Ldar), A(1, 2),                              //
                      B(Star), R(0),                                 //
                      B(LdaSmi8), U8(1),                             //
                      B(StoreICSloppy), R(0), U8(0),                 //
                                        U8((wide_idx_1 += 2))),      //
           B(Ldar), A(1, 2),                                         //
           B(Star), R(0),                                            //
           B(LdaSmi8), U8(2),                                        //
           B(StoreICSloppyWide), R(0), U16(0), U16(wide_idx_1 + 2),  //
           B(LdaUndefined),                                          //
           B(Return),                                                //
       },
       1,
       {"name"}},
      {"function f(a) {\n"
       " 'use strict';\n"
       "  a.name = 1;"
       REPEAT_127(SPACE, " a.name = 1; ")
       " a.name = 2; }\n"
       "f({name : \"test\"})\n",
       kPointerSize,
       2,
       1294,
       {
           B(Ldar), A(1, 2),                                         //
           B(Star), R(0),                                            //
           B(LdaSmi8), U8(1),                                        //
           B(StoreICStrict), R(0), U8(0), U8(wide_idx_2 += 2),       //
           REPEAT_127(COMMA,                                         //
                      B(Ldar), A(1, 2),                              //
                      B(Star), R(0),                                 //
                      B(LdaSmi8), U8(1),                             //
                      B(StoreICStrict), R(0), U8(0),                 //
                                        U8((wide_idx_2 += 2))),      //
           B(Ldar), A(1, 2),                                         //
           B(Star), R(0),                                            //
           B(LdaSmi8), U8(2),                                        //
           B(StoreICStrictWide), R(0), U16(0), U16(wide_idx_2 + 2),  //
           B(LdaUndefined),                                          //
           B(Return),                                                //
       },
       1,
       {"name"}},
      {"function f(a, b) {\n"
       " a[b] = 1;"
        REPEAT_127(SPACE, " a[b] = 1; ")
       " a[b] = 2; }\n"
       "f({name : \"test\"})\n",
       2 * kPointerSize,
       3,
       1809,
       {
           B(Ldar), A(1, 3),                                            //
           B(Star), R(0),                                               //
           B(Ldar), A(2, 3),                                            //
           B(Star), R(1),                                               //
           B(LdaSmi8), U8(1),                                           //
           B(KeyedStoreICSloppy), R(0), R(1), U8(wide_idx_3 += 2),      //
           REPEAT_127(COMMA,                                            //
                      B(Ldar), A(1, 3),                                 //
                      B(Star), R(0),                                    //
                      B(Ldar), A(2, 3),                                 //
                      B(Star), R(1),                                    //
                      B(LdaSmi8), U8(1),                                //
                      B(KeyedStoreICSloppy), R(0), R(1),                //
                                             U8((wide_idx_3 += 2))),    //
           B(Ldar), A(1, 3),                                            //
           B(Star), R(0),                                               //
           B(Ldar), A(2, 3),                                            //
           B(Star), R(1),                                               //
           B(LdaSmi8), U8(2),                                           //
           B(KeyedStoreICSloppyWide), R(0), R(1), U16(wide_idx_3 + 2),  //
           B(LdaUndefined),                                             //
           B(Return),                                                   //
       }},
      {"function f(a, b) {\n"
       " 'use strict';\n"
       "  a[b] = 1;"
        REPEAT_127(SPACE, " a[b] = 1; ")
       " a[b] = 2; }\n"
       "f({name : \"test\"})\n",
       2 * kPointerSize,
       3,
       1809,
       {
           B(Ldar), A(1, 3),                                            //
           B(Star), R(0),                                               //
           B(Ldar), A(2, 3),                                            //
           B(Star), R(1),                                               //
           B(LdaSmi8), U8(1),                                           //
           B(KeyedStoreICStrict), R(0), R(1), U8(wide_idx_4 += 2),      //
           REPEAT_127(COMMA,                                            //
                      B(Ldar), A(1, 3),                                 //
                      B(Star), R(0),                                    //
                      B(Ldar), A(2, 3),                                 //
                      B(Star), R(1),                                    //
                      B(LdaSmi8), U8(1),                                //
                      B(KeyedStoreICStrict), R(0), R(1),                //
                                             U8((wide_idx_4 += 2))),    //
           B(Ldar), A(1, 3),                                            //
           B(Star), R(0),                                               //
           B(Ldar), A(2, 3),                                            //
           B(Star), R(1),                                               //
           B(LdaSmi8), U8(2),                                           //
           B(KeyedStoreICStrictWide), R(0), R(1), U16(wide_idx_4 + 2),  //
           B(LdaUndefined),                                             //
           B(Return),                                                   //
       }}};
  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, helper.kFunctionName);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


#define FUNC_ARG "new (function Obj() { this.func = function() { return; }})()"


TEST(PropertyCall) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddCallICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  // These are a hack used by the CallWide test below.
  int wide_idx = vector->GetIndex(slot1) - 2;

  ExpectedSnippet<const char*> snippets[] = {
      {"function f(a) { return a.func(); }\nf(" FUNC_ARG ")",
       2 * kPointerSize,
       2,
       16,
       {
           B(Ldar), A(1, 2),                                           //
           B(Star), R(1),                                              //
           B(LoadICSloppy), R(1), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                              //
           B(Call), R(0), R(1), U8(0), U8(vector->GetIndex(slot1)),    //
           B(Return),                                                  //
       },
       1,
       {"func"}},
      {"function f(a, b, c) { return a.func(b, c); }\nf(" FUNC_ARG ", 1, 2)",
       4 * kPointerSize,
       4,
       24,
       {
           B(Ldar), A(1, 4),                                           //
           B(Star), R(1),                                              //
           B(LoadICSloppy), R(1), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                              //
           B(Ldar), A(2, 4),                                           //
           B(Star), R(2),                                              //
           B(Ldar), A(3, 4),                                           //
           B(Star), R(3),                                              //
           B(Call), R(0), R(1), U8(2), U8(vector->GetIndex(slot1)),    //
           B(Return)                                                   //
       },
       1,
       {"func"}},
      {"function f(a, b) { return a.func(b + b, b); }\nf(" FUNC_ARG ", 1)",
       4 * kPointerSize,
       3,
       30,
       {
           B(Ldar), A(1, 3),                                           //
           B(Star), R(1),                                              //
           B(LoadICSloppy), R(1), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                              //
           B(Ldar), A(2, 3),                                           //
           B(Star), R(3),                                              //
           B(Ldar), A(2, 3),                                           //
           B(Add), R(3),                                               //
           B(Star), R(2),                                              //
           B(Ldar), A(2, 3),                                           //
           B(Star), R(3),                                              //
           B(Call), R(0), R(1), U8(2), U8(vector->GetIndex(slot1)),    //
           B(Return),                                                  //
       },
       1,
       {"func"}},
      {"function f(a) {\n"
       " a.func;\n"
       REPEAT_127(SPACE, " a.func;\n")
       " return a.func(); }\nf(" FUNC_ARG ")",
       2 * kPointerSize,
       2,
       1044,
       {
           B(Ldar), A(1, 2),                                               //
           B(Star), R(0),                                                  //
           B(LoadICSloppy), R(0), U8(0), U8(wide_idx += 2),                //
           REPEAT_127(COMMA,                                               //
                      B(Ldar), A(1, 2),                                    //
                      B(Star), R(0),                                       //
                      B(LoadICSloppy), R(0), U8(0), U8((wide_idx += 2))),  //
           B(Ldar), A(1, 2),                                               //
           B(Star), R(1),                                                  //
           B(LoadICSloppyWide), R(1), U16(0), U16(wide_idx + 4),           //
           B(Star), R(0),                                                  //
           B(CallWide), R(0), R(1), U16(0), U16(wide_idx + 2),             //
           B(Return),                                                      //
       },
       1,
       {"func"}},
  };
  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, helper.kFunctionName);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(LoadGlobal) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  // These are a hack used by the LdaGlobalXXXWide tests below.
  int wide_idx_1 = vector->GetIndex(slot) - 2;
  int wide_idx_2 = vector->GetIndex(slot) - 2;

  ExpectedSnippet<const char*> snippets[] = {
      {"var a = 1;\nfunction f() { return a; }\nf()",
       0,
       1,
       4,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot)),  //
           B(Return)                                               //
       },
       1,
       {"a"}},
      {"function t() { }\nfunction f() { return t; }\nf()",
       0,
       1,
       4,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot)),  //
           B(Return)                                               //
       },
       1,
       {"t"}},
      {"'use strict'; var a = 1;\nfunction f() { return a; }\nf()",
       0,
       1,
       4,
       {
           B(LdaGlobalStrict), U8(0), U8(vector->GetIndex(slot)),  //
           B(Return)                                               //
       },
       1,
       {"a"}},
      {"a = 1;\nfunction f() { return a; }\nf()",
       0,
       1,
       4,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot)),  //
           B(Return)                                               //
       },
       1,
       {"a"}},
      {"a = 1;"
       "function f(b) {\n"
       "   b.name;\n"
        REPEAT_127(SPACE, "b.name; ")
       " return a;"
       "}\nf({name: 1});",
       kPointerSize,
       2,
       1030,
       {
           B(Ldar), A(1, 2),                                               //
           B(Star), R(0),                                                  //
           B(LoadICSloppy), R(0), U8(0), U8(wide_idx_1 += 2),              //
           REPEAT_127(COMMA,                                               //
                      B(Ldar), A(1, 2),                                    //
                      B(Star), R(0),                                       //
                      B(LoadICSloppy), R(0), U8(0), U8(wide_idx_1 += 2)),  //
           B(LdaGlobalSloppyWide), U16(1), U16(wide_idx_1 + 2),            //
           B(Return),                                                      //
       },
       2,
       {"name", "a"}},
      {"a = 1;"
       "function f(b) {\n"
       " 'use strict';\n"
       "  b.name\n"
          REPEAT_127(SPACE, "b.name; ")
       "  return a;"
       "}\nf({name: 1});",
       kPointerSize,
       2,
       1030,
       {
           B(Ldar), A(1, 2),                                               //
           B(Star), R(0),                                                  //
           B(LoadICStrict), R(0), U8(0), U8(wide_idx_2 += 2),              //
           REPEAT_127(COMMA,                                               //
                      B(Ldar), A(1, 2),                                    //
                      B(Star), R(0),                                       //
                      B(LoadICStrict), R(0), U8(0), U8(wide_idx_2 += 2)),  //
           B(LdaGlobalStrictWide), U16(1), U16(wide_idx_2 + 2),            //
           B(Return),                                                      //
       },
       2,
       {"name", "a"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(StoreGlobal) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  // These are a hack used by the StaGlobalXXXWide tests below.
  int wide_idx_1 = vector->GetIndex(slot) - 2;
  int wide_idx_2 = vector->GetIndex(slot) - 2;

  ExpectedSnippet<const char*> snippets[] = {
      {"var a = 1;\nfunction f() { a = 2; }\nf()",
       0,
       1,
       7,
       {
           B(LdaSmi8), U8(2),                                      //
           B(StaGlobalSloppy), U8(0), U8(vector->GetIndex(slot)),  //
           B(LdaUndefined),                                        //
           B(Return)                                               //
       },
       1,
       {"a"}},
      {"var a = \"test\"; function f(b) { a = b; }\nf(\"global\")",
       0,
       2,
       7,
       {
           B(Ldar), R(helper.kLastParamIndex),                     //
           B(StaGlobalSloppy), U8(0), U8(vector->GetIndex(slot)),  //
           B(LdaUndefined),                                        //
           B(Return)                                               //
       },
       1,
       {"a"}},
      {"'use strict'; var a = 1;\nfunction f() { a = 2; }\nf()",
       0,
       1,
       7,
       {
           B(LdaSmi8), U8(2),                                      //
           B(StaGlobalStrict), U8(0), U8(vector->GetIndex(slot)),  //
           B(LdaUndefined),                                        //
           B(Return)                                               //
       },
       1,
       {"a"}},
      {"a = 1;\nfunction f() { a = 2; }\nf()",
       0,
       1,
       7,
       {
           B(LdaSmi8), U8(2),                                      //
           B(StaGlobalSloppy), U8(0), U8(vector->GetIndex(slot)),  //
           B(LdaUndefined),                                        //
           B(Return)                                               //
       },
       1,
       {"a"}},
      {"a = 1;"
       "function f(b) {"
       " b.name;\n"
       REPEAT_127(SPACE, "b.name; ")
       " a = 2; }\n"
       "f({name: 1});",
       kPointerSize,
       2,
       1033,
       {
           B(Ldar), A(1, 2),                                               //
           B(Star), R(0),                                                  //
           B(LoadICSloppy), R(0), U8(0), U8(wide_idx_1 += 2),              //
           REPEAT_127(COMMA,                                               //
                      B(Ldar), A(1, 2),                                    //
                      B(Star), R(0),                                       //
                      B(LoadICSloppy), R(0), U8(0), U8(wide_idx_1 += 2)),  //
           B(LdaSmi8), U8(2),                                              //
           B(StaGlobalSloppyWide), U16(1), U16(wide_idx_1 + 2),            //
           B(LdaUndefined),                                                //
           B(Return),                                                      //
       },
       2,
       {"name", "a"}},
      {"a = 1;"
       "function f(b) {\n"
       " 'use strict';\n"
       "  b.name;\n"
       REPEAT_127(SPACE, "b.name; ")
       " a = 2; }\n"
       "f({name: 1});",
       kPointerSize,
       2,
       1033,
       {
           B(Ldar), A(1, 2),                                               //
           B(Star), R(0),                                                  //
           B(LoadICStrict), R(0), U8(0), U8(wide_idx_2 += 2),              //
           REPEAT_127(COMMA,                                               //
                      B(Ldar), A(1, 2),                                    //
                      B(Star), R(0),                                       //
                      B(LoadICStrict), R(0), U8(0), U8(wide_idx_2 += 2)),  //
           B(LdaSmi8), U8(2),                                              //
           B(StaGlobalStrictWide), U16(1), U16(wide_idx_2 + 2),            //
           B(LdaUndefined),                                                //
           B(Return),                                                      //
       },
       2,
       {"name", "a"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(CallGlobal) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddCallICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"function t() { }\nfunction f() { return t(); }\nf()",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaUndefined),                                          //
           B(Star), R(1),                                            //
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),   //
           B(Star), R(0),                                            //
           B(Call), R(0), R(1), U8(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                                 //
       },
       1,
       {"t"}},
      {"function t(a, b, c) { }\nfunction f() { return t(1, 2, 3); }\nf()",
       5 * kPointerSize,
       1,
       26,
       {
           B(LdaUndefined),                                          //
           B(Star), R(1),                                            //
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),   //
           B(Star), R(0),                                            //
           B(LdaSmi8), U8(1),                                        //
           B(Star), R(2),                                            //
           B(LdaSmi8), U8(2),                                        //
           B(Star), R(3),                                            //
           B(LdaSmi8), U8(3),                                        //
           B(Star), R(4),                                            //
           B(Call), R(0), R(1), U8(3), U8(vector->GetIndex(slot1)),  //
           B(Return)                                                 //
       },
       1,
       {"t"}},
  };

  size_t num_snippets = sizeof(snippets) / sizeof(snippets[0]);
  for (size_t i = 0; i < num_snippets; i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(CallRuntime) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<InstanceType> snippets[] = {
      {
          "function f() { %TheHole() }\nf()",
          0,
          1,
          7,
          {
              B(CallRuntime), U16(Runtime::kTheHole), R(0), U8(0),  //
              B(LdaUndefined),                                      //
              B(Return)                                             //
          },
      },
      {
          "function f(a) { return %IsArray(a) }\nf(undefined)",
          1 * kPointerSize,
          2,
          10,
          {
              B(Ldar), A(1, 2),                                     //
              B(Star), R(0),                                        //
              B(CallRuntime), U16(Runtime::kIsArray), R(0), U8(1),  //
              B(Return)                                             //
          },
      },
      {
          "function f() { return %Add(1, 2) }\nf()",
          2 * kPointerSize,
          1,
          14,
          {
              B(LdaSmi8), U8(1),                                //
              B(Star), R(0),                                    //
              B(LdaSmi8), U8(2),                                //
              B(Star), R(1),                                    //
              B(CallRuntime), U16(Runtime::kAdd), R(0), U8(2),  //
              B(Return)                                         //
          },
      },
      {
          "function f() { return %spread_iterable([1]) }\nf()",
          2 * kPointerSize,
          1,
          15,
          {
              B(LdaUndefined),                                              //
              B(Star), R(0),                                                //
              B(CreateArrayLiteral), U8(0), U8(0), U8(3),                   //
              B(Star), R(1),                                                //
              B(CallJSRuntime), U16(Context::SPREAD_ITERABLE_INDEX), R(0),  //
              U8(1),                                                        //
              B(Return),                                                    //
          },
          1,
          {InstanceType::FIXED_ARRAY_TYPE},
      },
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(IfConditions) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  Handle<Object> unused = helper.factory()->undefined_value();

  ExpectedSnippet<Handle<Object>> snippets[] = {
      {"function f() { if (0) { return 1; } else { return -1; } } f()",
       0,
       1,
       3,
       {
           B(LdaSmi8), U8(-1),  //
           B(Return),           //
       },
       0,
       {unused, unused, unused, unused, unused, unused}},
      {"function f() { if ('lucky') { return 1; } else { return -1; } } f();",
       0,
       1,
       3,
       {
           B(LdaSmi8), U8(1),  //
           B(Return),          //
       },
       0,
       {unused, unused, unused, unused, unused, unused}},
      {"function f() { if (false) { return 1; } else { return -1; } } f();",
       0,
       1,
       3,
       {
           B(LdaSmi8), U8(-1),  //
           B(Return),           //
       },
       0,
       {unused, unused, unused, unused, unused, unused}},
      {"function f() { if (false) { return 1; } } f();",
       0,
       1,
       2,
       {
           B(LdaUndefined),  //
           B(Return),        //
       },
       0,
       {unused, unused, unused, unused, unused, unused}},
      {"function f() { var a = 1; if (a) { a += 1; } else { return 2; } } f();",
       2 * kPointerSize,
       1,
       23,
       {
           B(LdaSmi8), U8(1),                //
           B(Star), R(0),                    //
           B(JumpIfToBooleanFalse), U8(14),  //
           B(Ldar), R(0),                    //
           B(Star), R(1),                    //
           B(LdaSmi8), U8(1),                //
           B(Add), R(1),                     //
           B(Star), R(0),                    //
           B(Jump), U8(5),                   //
           B(LdaSmi8), U8(2),                //
           B(Return),                        //
           B(LdaUndefined),                  //
           B(Return),                        //
       },
       0,
       {unused, unused, unused, unused, unused, unused}},
      {"function f(a) { if (a <= 0) { return 200; } else { return -200; } }"
       "f(99);",
       kPointerSize,
       2,
       17,
       {
           B(Ldar), A(1, 2),              //
           B(Star), R(0),                 //
           B(LdaZero),                    //
           B(TestLessThanOrEqual), R(0),  //
           B(JumpIfFalse), U8(5),         //
           B(LdaConstant), U8(0),         //
           B(Return),                     //
           B(LdaConstant), U8(1),         //
           B(Return),                     //
           B(LdaUndefined),               //
           B(Return),                     //
       },
       2,
       {helper.factory()->NewNumberFromInt(200),
        helper.factory()->NewNumberFromInt(-200), unused, unused, unused,
        unused}},
      {"function f(a, b) { if (a in b) { return 200; } }"
       "f('prop', { prop: 'yes'});",
       kPointerSize,
       3,
       15,
       {
           B(Ldar), A(1, 3),       //
           B(Star), R(0),          //
           B(Ldar), A(2, 3),       //
           B(TestIn), R(0),        //
           B(JumpIfFalse), U8(5),  //
           B(LdaConstant), U8(0),  //
           B(Return),              //
           B(LdaUndefined),        //
           B(Return),              //
       },
       1,
       {helper.factory()->NewNumberFromInt(200), unused, unused, unused, unused,
        unused}},
      {"function f(z) { var a = 0; var b = 0; if (a === 0.01) { "
       REPEAT_64(SPACE, "b = a; a = b; ")
       " return 200; } else { return -200; } } f(0.001)",
       3 * kPointerSize,
       2,
       282,
       {
           B(LdaZero),                     //
           B(Star), R(0),                  //
           B(LdaZero),                     //
           B(Star), R(1),                  //
           B(Ldar), R(0),                  //
           B(Star), R(2),                  //
           B(LdaConstant), U8(0),          //
           B(TestEqualStrict), R(2),       //
           B(JumpIfFalseConstant), U8(2),  //
           B(Ldar), R(0),                  //
           REPEAT_64(COMMA,                //
             B(Star), R(1),                //
             B(Star), R(0)),               //
           B(LdaConstant), U8(1),          //
           B(Return),                      //
           B(LdaConstant), U8(3),          //
           B(Return),                      //
           B(LdaUndefined),                //
           B(Return)},                     //
       4,
       {helper.factory()->NewHeapNumber(0.01),
        helper.factory()->NewNumberFromInt(200),
        helper.factory()->NewNumberFromInt(263),
        helper.factory()->NewNumberFromInt(-200), unused, unused}},
      {"function f() { var a = 0; var b = 0; if (a) { "
       REPEAT_64(SPACE, "b = a; a = b; ")
       " return 200; } else { return -200; } } f()",
       2 * kPointerSize,
       1,
       276,
       {
           B(LdaZero),                              //
           B(Star), R(0),                           //
           B(LdaZero),                              //
           B(Star), R(1),                           //
           B(Ldar), R(0),                           //
           B(JumpIfToBooleanFalseConstant), U8(1),  //
           B(Ldar), R(0),                           //
           REPEAT_64(COMMA,                         //
             B(Star), R(1),                         //
             B(Star), R(0)),                        //
           B(LdaConstant), U8(0),                   //
           B(Return),                               //
           B(LdaConstant), U8(2),                   //
           B(Return),                               //
           B(LdaUndefined),                         //
           B(Return)},                              //
       3,
       {helper.factory()->NewNumberFromInt(200),
        helper.factory()->NewNumberFromInt(263),
        helper.factory()->NewNumberFromInt(-200), unused, unused, unused}},

      {"function f(a, b) {\n"
       "  if (a == b) { return 1; }\n"
       "  if (a === b) { return 1; }\n"
       "  if (a < b) { return 1; }\n"
       "  if (a > b) { return 1; }\n"
       "  if (a <= b) { return 1; }\n"
       "  if (a >= b) { return 1; }\n"
       "  if (a in b) { return 1; }\n"
       "  if (a instanceof b) { return 1; }\n"
       "  return 0;\n"
       "} f(1, 1);",
       kPointerSize,
       3,
       106,
       {
#define IF_CONDITION_RETURN(condition) \
         B(Ldar), A(1, 3),             \
         B(Star), R(0),                \
         B(Ldar), A(2, 3),             \
         B(condition), R(0),           \
         B(JumpIfFalse), U8(5),        \
         B(LdaSmi8), U8(1),            \
         B(Return),
           IF_CONDITION_RETURN(TestEqual)               //
           IF_CONDITION_RETURN(TestEqualStrict)         //
           IF_CONDITION_RETURN(TestLessThan)            //
           IF_CONDITION_RETURN(TestGreaterThan)         //
           IF_CONDITION_RETURN(TestLessThanOrEqual)     //
           IF_CONDITION_RETURN(TestGreaterThanOrEqual)  //
           IF_CONDITION_RETURN(TestIn)                  //
           IF_CONDITION_RETURN(TestInstanceOf)          //
           B(LdaZero),                                  //
           B(Return)},                                  //
#undef IF_CONDITION_RETURN
       0,
       {unused, unused, unused, unused, unused, unused}},
      {"function f() {"
       " var a = 0;"
       " if (a) {"
       "  return 20;"
       "} else {"
       "  return -20;}"
       "};"
       "f();",
       1 * kPointerSize,
       1,
       13,
       {
           B(LdaZero),                      //
           B(Star), R(0),                   //
           B(JumpIfToBooleanFalse), U8(5),  //
           B(LdaSmi8), U8(20),              //
           B(Return),                       //
           B(LdaSmi8), U8(-20),             //
           B(Return),                       //
           B(LdaUndefined),                 //
           B(Return)
       },
       0,
       {unused, unused, unused, unused, unused, unused}}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, helper.kFunctionName);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(DeclareGlobals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  // Create different feedback vector specs to be precise on slot numbering.
  FeedbackVectorSpec feedback_spec_stores(&zone);
  FeedbackVectorSlot store_slot_1 = feedback_spec_stores.AddStoreICSlot();
  FeedbackVectorSlot store_slot_2 = feedback_spec_stores.AddStoreICSlot();
  USE(store_slot_1);

  Handle<i::TypeFeedbackVector> store_vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec_stores);

  FeedbackVectorSpec feedback_spec_loads(&zone);
  FeedbackVectorSlot load_slot_1 = feedback_spec_loads.AddLoadICSlot();
  FeedbackVectorSlot call_slot_1 = feedback_spec_loads.AddCallICSlot();

  Handle<i::TypeFeedbackVector> load_vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec_loads);

  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a = 1;",
       4 * kPointerSize,
       1,
       30,
       {
           B(LdaConstant), U8(0),                                            //
           B(Star), R(1),                                                    //
           B(LdaZero),                                                       //
           B(Star), R(2),                                                    //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(1), U8(2),       //
           B(LdaConstant), U8(1),                                            //
           B(Star), R(1),                                                    //
           B(LdaZero),                                                       //
           B(Star), R(2),                                                    //
           B(LdaSmi8), U8(1),                                                //
           B(Star), R(3),                                                    //
           B(CallRuntime), U16(Runtime::kInitializeVarGlobal), R(1), U8(3),  //
           B(LdaUndefined),                                                  //
           B(Return)                                                         //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"function f() {}",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaConstant), U8(0),                                       //
           B(Star), R(0),                                               //
           B(LdaZero),                                                  //
           B(Star), R(1),                                               //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(0), U8(2),  //
           B(LdaUndefined),                                             //
           B(Return)                                                    //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1;\na=2;",
       4 * kPointerSize,
       1,
       36,
       {
           B(LdaConstant), U8(0),                                            //
           B(Star), R(1),                                                    //
           B(LdaZero),                                                       //
           B(Star), R(2),                                                    //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(1), U8(2),       //
           B(LdaConstant), U8(1),                                            //
           B(Star), R(1),                                                    //
           B(LdaZero),                                                       //
           B(Star), R(2),                                                    //
           B(LdaSmi8), U8(1),                                                //
           B(Star), R(3),                                                    //
           B(CallRuntime), U16(Runtime::kInitializeVarGlobal), R(1), U8(3),  //
           B(LdaSmi8), U8(2),                                                //
           B(StaGlobalSloppy), U8(1),                                        //
                               U8(store_vector->GetIndex(store_slot_2)),     //
           B(Star), R(0),                                                    //
           B(Return)                                                         //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"function f() {}\nf();",
       3 * kPointerSize,
       1,
       28,
       {
           B(LdaConstant), U8(0),                                        //
           B(Star), R(1),                                                //
           B(LdaZero),                                                   //
           B(Star), R(2),                                                //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(1), U8(2),   //
           B(LdaUndefined),                                              //
           B(Star), R(2),                                                //
           B(LdaGlobalSloppy), U8(1),                                    //
                               U8(load_vector->GetIndex(load_slot_1)),   //
           B(Star), R(1),                                                //
           B(Call), R(1), R(2), U8(0),                                   //
                                U8(load_vector->GetIndex(call_slot_1)),  //
           B(Star), R(0),                                                //
           B(Return)                                                     //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeTopLevelBytecode(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(BreakableBlocks) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var x = 0;\n"
       "label: {\n"
       "  x = x + 1;\n"
       "  break label;\n"
       "  x = x + 1;\n"
       "}\n"
       "return x;",
       2 * kPointerSize,
       1,
       16,
       {
           B(LdaZero),         //
           B(Star), R(0),      //
           B(Star), R(1),      //
           B(LdaSmi8), U8(1),  //
           B(Add), R(1),       //
           B(Star), R(0),      //
           B(Jump), U8(2),     //
           B(Ldar), R(0),      //
           B(Return)           //
       }},
      {"var sum = 0;\n"
       "outer: {\n"
       "  for (var x = 0; x < 10; ++x) {\n"
       "    for (var y = 0; y < 3; ++y) {\n"
       "      ++sum;\n"
       "      if (x + y == 12) { break outer; }\n"
       "    }\n"
       "  }\n"
       "}\n"
       "return sum;",
       5 * kPointerSize,
       1,
       72,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(LdaZero),              //
           B(Star), R(1),           //
           B(Ldar), R(1),           //
           B(Star), R(3),           //
           B(LdaSmi8), U8(10),      //
           B(TestLessThan), R(3),   //
           B(JumpIfFalse), U8(55),  //
           B(LdaZero),              //
           B(Star), R(2),           //
           B(Ldar), R(2),           //
           B(Star), R(3),           //
           B(LdaSmi8), U8(3),       //
           B(TestLessThan), R(3),   //
           B(JumpIfFalse), U8(34),  //
           B(Ldar), R(0),           //
           B(ToNumber),             //
           B(Inc),                  //
           B(Star), R(0),           //
           B(Ldar), R(1),           //
           B(Star), R(3),           //
           B(Ldar), R(2),           //
           B(Add), R(3),            //
           B(Star), R(4),           //
           B(LdaSmi8), U8(12),      //
           B(TestEqual), R(4),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(18),         //
           B(Ldar), R(2),           //
           B(ToNumber),             //
           B(Inc),                  //
           B(Star), R(2),           //
           B(Jump), U8(-40),        //
           B(Ldar), R(1),           //
           B(ToNumber),             //
           B(Inc),                  //
           B(Star), R(1),           //
           B(Jump), U8(-61),        //
           B(Ldar), R(0),           //
           B(Return),               //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(BasicLoops) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var x = 0;\n"
       "while (false) { x = 99; break; continue; }\n"
       "return x;",
       1 * kPointerSize,
       1,
       4,
       {
           B(LdaZero),     //
           B(Star), R(0),  //
           B(Return)       //
       }},
      {"var x = 0;"
       "while (false) {"
       "  x = x + 1;"
       "};"
       "return x;",
       1 * kPointerSize,
       1,
       4,
       {
           B(LdaZero),     //
           B(Star), R(0),  //
           B(Return),      //
       },
       0},
      {"var x = 0;"
       "var y = 1;"
       "while (x < 10) {"
       "  y = y * 12;"
       "  x = x + 1;"
       "  if (x == 3) continue;"
       "  if (x == 4) break;"
       "}"
       "return y;",
       3 * kPointerSize,
       1,
       64,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(LdaSmi8), U8(1),       //
           B(Star), R(1),           //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(10),      //
           B(TestLessThan), R(2),   //
           B(JumpIfFalse), U8(46),  //
           B(Ldar), R(1),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(12),      //
           B(Mul), R(2),            //
           B(Star), R(1),           //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(2),            //
           B(Star), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(3),       //
           B(TestEqual), R(2),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(-38),        //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(4),       //
           B(TestEqual), R(2),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(4),          //
           B(Jump), U8(-52),        //
           B(Ldar), R(1),           //
           B(Return),               //
       },
       0},
      {"var i = 0;"
       "while (true) {"
       "  if (i < 0) continue;"
       "  if (i == 3) break;"
       "  if (i == 4) break;"
       "  if (i == 10) continue;"
       "  if (i == 5) break;"
       "  i = i + 1;"
       "}"
       "return i;",
       2 * kPointerSize,
       1,
       77,
       {
           B(LdaZero),             //
           B(Star), R(0),          //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaZero),             //
           B(TestLessThan), R(1),  //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(-9),        //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(3),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(50),        //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(4),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(38),        //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(10),     //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(-45),       //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(5),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(14),        //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(Add), R(1),           //
           B(Star), R(0),          //
           B(Jump), U8(-69),       //
           B(Ldar), R(0),          //
           B(Return),              //
       },
       0},
      {"var i = 0;"
       "while (true) {"
       "  while (i < 3) {"
       "    if (i == 2) break;"
       "    i = i + 1;"
       "  }"
       "  i = i + 1;"
       "  break;"
       "}"
       "return i;",
       2 * kPointerSize,
       1,
       54,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(3),       //
           B(TestLessThan), R(1),   //
           B(JumpIfFalse), U8(26),  //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(2),       //
           B(TestEqual), R(1),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(14),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(1),            //
           B(Star), R(0),           //
           B(Jump), U8(-32),        //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(1),            //
           B(Star), R(0),           //
           B(Jump), U8(4),          //
           B(Jump), U8(-46),        //
           B(Ldar), R(0),           //
           B(Return),               //
       },
       0},
      {"var x = 10;"
       "var y = 1;"
       "while (x) {"
       "  y = y * 12;"
       "  x = x - 1;"
       "}"
       "return y;",
       3 * kPointerSize,
       1,
       37,
       {
           B(LdaSmi8), U8(10),               //
           B(Star), R(0),                    //
           B(LdaSmi8), U8(1),                //
           B(Star), R(1),                    //
           B(Ldar), R(0),                    //
           B(JumpIfToBooleanFalse), U8(24),  //
           B(Ldar), R(1),                    //
           B(Star), R(2),                    //
           B(LdaSmi8), U8(12),               //
           B(Mul), R(2),                     //
           B(Star), R(1),                    //
           B(Ldar), R(0),                    //
           B(Star), R(2),                    //
           B(LdaSmi8), U8(1),                //
           B(Sub), R(2),                     //
           B(Star), R(0),                    //
           B(Jump), U8(-24),                 //
           B(Ldar), R(1),                    //
           B(Return),                        //
       },
       0},
      {"var x = 0; var y = 1;"
       "do {"
       "  y = y * 10;"
       "  if (x == 5) break;"
       "  if (x == 6) continue;"
       "  x = x + 1;"
       "} while (x < 10);"
       "return y;",
       3 * kPointerSize,
       1,
       64,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(LdaSmi8), U8(1),       //
           B(Star), R(1),           //
           B(Ldar), R(1),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(10),      //
           B(Mul), R(2),            //
           B(Star), R(1),           //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(5),       //
           B(TestEqual), R(2),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(34),         //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(6),       //
           B(TestEqual), R(2),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(12),         //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(2),            //
           B(Star), R(0),           //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(10),      //
           B(TestLessThan), R(2),   //
           B(JumpIfTrue), U8(-52),  //
           B(Ldar), R(1),           //
           B(Return),               //
       },
       0},
      {"var x = 10;"
       "var y = 1;"
       "do {"
       "  y = y * 12;"
       "  x = x - 1;"
       "} while (x);"
       "return y;",
       3 * kPointerSize,
       1,
       35,
       {
           B(LdaSmi8), U8(10),               //
           B(Star), R(0),                    //
           B(LdaSmi8), U8(1),                //
           B(Star), R(1),                    //
           B(Ldar), R(1),                    //
           B(Star), R(2),                    //
           B(LdaSmi8), U8(12),               //
           B(Mul), R(2),                     //
           B(Star), R(1),                    //
           B(Ldar), R(0),                    //
           B(Star), R(2),                    //
           B(LdaSmi8), U8(1),                //
           B(Sub), R(2),                     //
           B(Star), R(0),                    //
           B(Ldar), R(0),                    //
           B(JumpIfToBooleanTrue), U8(-22),  //
           B(Ldar), R(1),                    //
           B(Return),                        //
       },
       0},
      {"var x = 0; var y = 1;"
       "do {"
       "  y = y * 10;"
       "  if (x == 5) break;"
       "  x = x + 1;"
       "  if (x == 6) continue;"
       "} while (false);"
       "return y;",
       3 * kPointerSize,
       1,
       52,
       {
           B(LdaZero),             //
           B(Star), R(0),          //
           B(LdaSmi8), U8(1),      //
           B(Star), R(1),          //
           B(Ldar), R(1),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(10),     //
           B(Mul), R(2),           //
           B(Star), R(1),          //
           B(Ldar), R(0),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(5),      //
           B(TestEqual), R(2),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(22),        //
           B(Ldar), R(0),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(1),      //
           B(Add), R(2),           //
           B(Star), R(0),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(6),      //
           B(TestEqual), R(2),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(2),         //
           B(Ldar), R(1),          //
           B(Return),              //
       },
       0},
      {"var x = 0; var y = 1;"
       "do {"
       "  y = y * 10;"
       "  if (x == 5) break;"
       "  x = x + 1;"
       "  if (x == 6) continue;"
       "} while (true);"
       "return y;",
       3 * kPointerSize,
       1,
       54,
       {
           B(LdaZero),             //
           B(Star), R(0),          //
           B(LdaSmi8), U8(1),      //
           B(Star), R(1),          //
           B(Ldar), R(1),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(10),     //
           B(Mul), R(2),           //
           B(Star), R(1),          //
           B(Ldar), R(0),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(5),      //
           B(TestEqual), R(2),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(24),        //
           B(Ldar), R(0),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(1),      //
           B(Add), R(2),           //
           B(Star), R(0),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(6),      //
           B(TestEqual), R(2),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(-40),       //
           B(Jump), U8(-42),       //
           B(Ldar), R(1),          //
           B(Return),              //
       },
       0},
      {"var x = 0; "
       "for (;;) {"
       "  if (x == 1) break;"
       "  if (x == 2) continue;"
       "  x = x + 1;"
       "}",
       2 * kPointerSize,
       1,
       41,
       {
           B(LdaZero),             //
           B(Star), R(0),          //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(26),        //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(2),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(-22),       //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(Add), R(1),           //
           B(Star), R(0),          //
           B(Jump), U8(-34),       //
           B(LdaUndefined),        //
           B(Return),              //
       },
       0},
      {"for (var x = 0;;) {"
       "  if (x == 1) break;"
       "  if (x == 2) continue;"
       "  x = x + 1;"
       "}",
       2 * kPointerSize,
       1,
       41,
       {
           B(LdaZero),             //
           B(Star), R(0),          //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(26),        //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(2),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(-22),       //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(Add), R(1),           //
           B(Star), R(0),          //
           B(Jump), U8(-34),       //
           B(LdaUndefined),        //
           B(Return),              //
       },
       0},
      {"var x = 0; "
       "for (;; x = x + 1) {"
       "  if (x == 1) break;"
       "  if (x == 2) continue;"
       "}",
       2 * kPointerSize,
       1,
       41,
       {
           B(LdaZero),             //
           B(Star), R(0),          //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(26),        //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(2),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(2),         //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(Add), R(1),           //
           B(Star), R(0),          //
           B(Jump), U8(-34),       //
           B(LdaUndefined),        //
           B(Return),              //
       },
       0},
      {"for (var x = 0;; x = x + 1) {"
       "  if (x == 1) break;"
       "  if (x == 2) continue;"
       "}",
       2 * kPointerSize,
       1,
       41,
       {
           B(LdaZero),             //
           B(Star), R(0),          //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(26),        //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(2),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(2),         //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(Add), R(1),           //
           B(Star), R(0),          //
           B(Jump), U8(-34),       //
           B(LdaUndefined),        //
           B(Return),              //
       },
       0},
      {"var u = 0;"
       "for (var i = 0; i < 100; i = i + 1) {"
       "  u = u + 1;"
       "  continue;"
       "}",
       3 * kPointerSize,
       1,
       42,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(LdaZero),              //
           B(Star), R(1),           //
           B(Ldar), R(1),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(100),     //
           B(TestLessThan), R(2),   //
           B(JumpIfFalse), U8(26),  //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(2),            //
           B(Star), R(0),           //
           B(Jump), U8(2),          //
           B(Ldar), R(1),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(2),            //
           B(Star), R(1),           //
           B(Jump), U8(-32),        //
           B(LdaUndefined),         //
           B(Return),               //
       },
       0},
      {"var y = 1;"
       "for (var x = 10; x; --x) {"
       "  y = y * 12;"
       "}"
       "return y;",
       3 * kPointerSize,
       1,
       33,
       {
           B(LdaSmi8), U8(1),                //
           B(Star), R(0),                    //
           B(LdaSmi8), U8(10),               //
           B(Star), R(1),                    //
           B(Ldar), R(1),                    //
           B(JumpIfToBooleanFalse), U8(20),  //
           B(Ldar), R(0),                    //
           B(Star), R(2),                    //
           B(LdaSmi8), U8(12),               //
           B(Mul), R(2),                     //
           B(Star), R(0),                    //
           B(Ldar), R(1),                    //
           B(ToNumber),                      //
           B(Dec),                           //
           B(Star), R(1),                    //
           B(Jump), U8(-20),                 //
           B(Ldar), R(0),                    //
           B(Return),                        //
       },
       0},
      {"var x = 0;"
       "for (var i = 0; false; i++) {"
       "  x = x + 1;"
       "};"
       "return x;",
       2 * kPointerSize,
       1,
       9,
       {
           B(LdaZero),     //
           B(Star), R(0),  //
           B(LdaZero),     //
           B(Star), R(1),  //
           B(Ldar), R(0),  //
           B(Return),      //
       },
       0},
      {"var x = 0;"
       "for (var i = 0; true; ++i) {"
       "  x = x + 1;"
       "  if (x == 20) break;"
       "};"
       "return x;",
       3 * kPointerSize,
       1,
       37,
       {
           B(LdaZero),             //
           B(Star), R(0),          //
           B(LdaZero),             //
           B(Star), R(1),          //
           B(Ldar), R(0),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(1),      //
           B(Add), R(2),           //
           B(Star), R(0),          //
           B(Star), R(2),          //
           B(LdaSmi8), U8(20),     //
           B(TestEqual), R(2),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(10),        //
           B(Ldar), R(1),          //
           B(ToNumber),            //
           B(Inc),                 //
           B(Star), R(1),          //
           B(Jump), U8(-26),       //
           B(Ldar), R(0),          //
           B(Return),              //
       },
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(JumpsRequiringConstantWideOperands) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int constant_count = 0;
  ExpectedSnippet<Handle<Object>, 316> snippets[] = {
      {
       REPEAT_256(SPACE, "var x = 0.1;")
       REPEAT_32(SPACE, "var x = 0.2;")
       REPEAT_16(SPACE, "var x = 0.3;")
       REPEAT_8(SPACE, "var x = 0.4;")
       "for (var i = 0; i < 3; i++) {\n"
       "  if (i == 1) continue;\n"
       "  if (i == 2) break;\n"
       "}\n"
       "return 3;",
       kPointerSize * 3,
       1,
       1359,
       {
#define L(c) B(LdaConstant), U8(c), B(Star), R(0)
           REPEAT_256(COMMA, L(constant_count++)),
#undef L
#define LW(c) B(LdaConstantWide), U16I(c), B(Star), R(0)
           REPEAT_32(COMMA, LW(constant_count)),
           REPEAT_16(COMMA, LW(constant_count)),
           REPEAT_8(COMMA, LW(constant_count)),
#undef LW
           B(LdaZero),                            //
           B(Star), R(1),                         //
           B(Ldar), R(1),                         //
           B(Star), R(2),                         //
           B(LdaSmi8), U8(3),                     //
           B(TestLessThan), R(2),                 //
           B(JumpIfFalseConstantWide), U16(313),  //
           B(Ldar), R(1),                         //
           B(Star), R(2),                         //
           B(LdaSmi8), U8(1),                     //
           B(TestEqual), R(2),                    //
           B(JumpIfFalseConstantWide), U16(312),  //
           B(JumpConstantWide), U16(315),         //
           B(Ldar), R(1),                         //
           B(Star), R(2),                         //
           B(LdaSmi8), U8(2),                     //
           B(TestEqual), R(2),                    //
           B(JumpIfFalseConstantWide), U16(312),  //
           B(JumpConstantWide), U16(314),         //
           B(Ldar), R(1),                         //
           B(ToNumber),                           //
           B(Star), R(2),                         //
           B(Inc),                                //
           B(Star), R(1),                         //
           B(Jump), U8(-47),                      //
           B(LdaSmi8), U8(3),                     //
           B(Return)                              //
       },
       316,
       {
#define S(x) CcTest::i_isolate()->factory()->NewNumber(x)
        REPEAT_256(COMMA, S(0.1)),
        REPEAT_32(COMMA, S(0.2)),
        REPEAT_16(COMMA, S(0.3)),
        REPEAT_8(COMMA, S(0.4)),
#undef S
#define N(x) CcTest::i_isolate()->factory()->NewNumberFromInt(x)
           N(6), N(41), N(13), N(17)
#undef N
       }}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(UnaryOperators) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var x = 0;"
       "while (x != 10) {"
       "  x = x + 10;"
       "}"
       "return x;",
       2 * kPointerSize,
       1,
       29,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(10),      //
           B(TestEqual), R(1),      //
           B(LogicalNot),           //
           B(JumpIfFalse), U8(14),  //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(10),      //
           B(Add), R(1),            //
           B(Star), R(0),           //
           B(Jump), U8(-21),        //
           B(Ldar), R(0),           //
           B(Return),               //
       },
       0},
      {"var x = false;"
       "do {"
       "  x = !x;"
       "} while(x == false);"
       "return x;",
       2 * kPointerSize,
       1,
       20,
       {
           B(LdaFalse),            //
           B(Star), R(0),          //
           B(Ldar), R(0),          //
           B(LogicalNot),          //
           B(Star), R(0),          //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaFalse),            //
           B(TestEqual), R(1),     //
           B(JumpIfTrue), U8(-12),  //
           B(Ldar), R(0),          //
           B(Return),              //
       },
       0},
      {"var x = 101;"
       "return void(x * 3);",
       2 * kPointerSize,
       1,
       12,
       {
           B(LdaSmi8), U8(101),  //
           B(Star), R(0),        //
           B(Star), R(1),        //
           B(LdaSmi8), U8(3),    //
           B(Mul), R(1),         //
           B(LdaUndefined),      //
           B(Return),            //
       },
       0},
      {"var x = 1234;"
       "var y = void (x * x - 1);"
       "return y;",
       4 * kPointerSize,
       1,
       20,
       {
           B(LdaConstant), U8(0),  //
           B(Star), R(0),          //
           B(Star), R(2),          //
           B(Ldar), R(0),          //
           B(Mul), R(2),           //
           B(Star), R(3),          //
           B(LdaSmi8), U8(1),      //
           B(Sub), R(3),           //
           B(LdaUndefined),        //
           B(Star), R(1),          //
           B(Return),              //
       },
       1,
       {1234}},
      {"var x = 13;"
       "return ~x;",
       2 * kPointerSize,
       1,
       11,
       {
           B(LdaSmi8), U8(13),   //
           B(Star), R(0),        //
           B(Star), R(1),        //
           B(LdaSmi8), U8(-1),   //
           B(BitwiseXor), R(1),  //
           B(Return),            //
       },
       0},
      {"var x = 13;"
       "return +x;",
       2 * kPointerSize,
       1,
       11,
       {
           B(LdaSmi8), U8(13),  //
           B(Star), R(0),       //
           B(Star), R(1),       //
           B(LdaSmi8), U8(1),   //
           B(Mul), R(1),        //
           B(Return),           //
       },
       0},
      {"var x = 13;"
       "return -x;",
       2 * kPointerSize,
       1,
       11,
       {
           B(LdaSmi8), U8(13),  //
           B(Star), R(0),       //
           B(Star), R(1),       //
           B(LdaSmi8), U8(-1),  //
           B(Mul), R(1),        //
           B(Return),           //
       },
       0}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(Typeof) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"function f() {\n"
       " var x = 13;\n"
       " return typeof(x);\n"
       "}; f();",
       kPointerSize,
       1,
       6,
       {
           B(LdaSmi8), U8(13),  //
           B(Star), R(0),       //
           B(TypeOf),           //
           B(Return),           //
       }},
      {"var x = 13;\n"
       "function f() {\n"
       " return typeof(x);\n"
       "}; f();",
       0,
       1,
       5,
       {
           B(LdaGlobalInsideTypeofSloppy), U8(0),                       //
                                           U8(vector->GetIndex(slot)),  //
           B(TypeOf),                                                   //
           B(Return),                                                   //
       },
       1,
       {"x"}},
      {"var x = 13;\n"
       "function f() {\n"
       " 'use strict';\n"
       " return typeof(x);\n"
       "}; f();",
       0,
       1,
       5,
       {
           B(LdaGlobalInsideTypeofStrict), U8(0),                       //
                                           U8(vector->GetIndex(slot)),  //
           B(TypeOf),                                                   //
           B(Return),                                                   //
       },
       1,
       {"x"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunction(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(Delete) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int deep_elements_flags =
      ObjectLiteral::kFastElements | ObjectLiteral::kDisableMementos;
  int closure = Register::function_closure().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;

  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a = {x:13, y:14}; return delete a.x;",
       2 * kPointerSize,
       1,
       13,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Star), R(0),                                                  //
           B(Star), R(1),                                                  //
           B(LdaConstant), U8(1),                                          //
           B(DeletePropertySloppy), R(1),                                  //
           B(Return)
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"'use strict'; var a = {x:13, y:14}; return delete a.x;",
       2 * kPointerSize,
       1,
       13,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Star), R(0),                                                  //
           B(Star), R(1),                                                  //
           B(LdaConstant), U8(1),                                          //
           B(DeletePropertyStrict), R(1),                                  //
           B(Return)
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var a = {1:13, 2:14}; return delete a[2];",
       2 * kPointerSize,
       1,
       13,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Star), R(0),                                                  //
           B(Star), R(1),                                                  //
           B(LdaSmi8), U8(2),                                              //
           B(DeletePropertySloppy), R(1),                                  //
           B(Return)
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 10; return delete a;",
       1 * kPointerSize,
       1,
       6,
       {
           B(LdaSmi8), U8(10),  //
           B(Star), R(0),       //
           B(LdaFalse),         //
           B(Return)
        },
       0},
      {"'use strict';"
       "var a = {1:10};"
       "(function f1() {return a;});"
       "return delete a[1];",
       2 * kPointerSize,
       1,
       27,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),              //
                            R(closure), U8(1),                             //
           B(PushContext), R(0),                                           //
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(StaContextSlot), R(0), U8(first_context_slot),                //
           B(CreateClosure), U8(1), U8(0),                                 //
           B(LdaContextSlot), R(0), U8(first_context_slot),                //
           B(Star), R(1),                                                  //
           B(LdaSmi8), U8(1),                                              //
           B(DeletePropertyStrict), R(1),                                  //
           B(Return)
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return delete 'test';",
       0 * kPointerSize,
       1,
       2,
       {
           B(LdaTrue),  //
           B(Return)
       },
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(GlobalDelete) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int context = Register::function_context().index();
  int native_context_index = Context::NATIVE_CONTEXT_INDEX;
  int global_context_index = Context::EXTENSION_INDEX;
  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a = {x:13, y:14};\n function f() { return delete a.x; };\n f();",
       1 * kPointerSize,
       1,
       10,
       {B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot)),  //
        B(Star), R(0),                                          //
        B(LdaConstant), U8(1),                                  //
        B(DeletePropertySloppy), R(0),                          //
        B(Return)},
       2,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"a = {1:13, 2:14};\n"
       "function f() {'use strict'; return delete a[1];};\n f();",
       1 * kPointerSize,
       1,
       10,
       {B(LdaGlobalStrict), U8(0), U8(vector->GetIndex(slot)),  //
        B(Star), R(0),                                          //
        B(LdaSmi8), U8(1),                                      //
        B(DeletePropertyStrict), R(0),                          //
        B(Return)},
       1,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var a = {x:13, y:14};\n function f() { return delete a; };\n f();",
       2 * kPointerSize,
       1,
       15,
       {B(LdaContextSlot), R(context), U8(native_context_index),  //
        B(Star), R(0),                                            //
        B(LdaContextSlot), R(0), U8(global_context_index),        //
        B(Star), R(1),                                            //
        B(LdaConstant), U8(0),                                    //
        B(DeletePropertySloppy), R(1),                            //
        B(Return)},
       1,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"b = 30;\n function f() { return delete b; };\n f();",
       2 * kPointerSize,
       1,
       15,
       {B(LdaContextSlot), R(context), U8(native_context_index),  //
        B(Star), R(0),                                            //
        B(LdaContextSlot), R(0), U8(global_context_index),        //
        B(Star), R(1),                                            //
        B(LdaConstant), U8(0),                                    //
        B(DeletePropertySloppy), R(1),                            //
        B(Return)},
       1,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(FunctionLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot = feedback_spec.AddCallICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<InstanceType> snippets[] = {
      {"return function(){ }",
       0,
       1,
       4,
       {
           B(CreateClosure), U8(0), U8(0),  //
           B(Return)                        //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return (function(){ })()",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaUndefined),                                         //
           B(Star), R(1),                                           //
           B(CreateClosure), U8(0), U8(0),                          //
           B(Star), R(0),                                           //
           B(Call), R(0), R(1), U8(0), U8(vector->GetIndex(slot)),  //
           B(Return)                                                //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return (function(x){ return x; })(1)",
       3 * kPointerSize,
       1,
       18,
       {
           B(LdaUndefined),                                         //
           B(Star), R(1),                                           //
           B(CreateClosure), U8(0), U8(0),                          //
           B(Star), R(0),                                           //
           B(LdaSmi8), U8(1),                                       //
           B(Star), R(2),                                           //
           B(Call), R(0), R(1), U8(1), U8(vector->GetIndex(slot)),  //
           B(Return)                                                //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(RegExpLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddCallICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddLoadICSlot();
  uint8_t i_flags = JSRegExp::kIgnoreCase;

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"return /ab+d/;",
       0 * kPointerSize,
       1,
       5,
       {
           B(CreateRegExpLiteral), U8(0), U8(0), U8(0),  //
           B(Return),                                    //
       },
       1,
       {"ab+d"}},
      {"return /(\\w+)\\s(\\w+)/i;",
       0 * kPointerSize,
       1,
       5,
       {
           B(CreateRegExpLiteral), U8(0), U8(0), U8(i_flags),  //
           B(Return),                                          //
       },
       1,
       {"(\\w+)\\s(\\w+)"}},
      {"return /ab+d/.exec('abdd');",
       3 * kPointerSize,
       1,
       22,
       {
           B(CreateRegExpLiteral), U8(0), U8(0), U8(0),                //
           B(Star), R(1),                                              //
           B(LoadICSloppy), R(1), U8(1), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                              //
           B(LdaConstant), U8(2),                                      //
           B(Star), R(2),                                              //
           B(Call), R(0), R(1), U8(1), U8(vector->GetIndex(slot1)),    //
           B(Return),                                                  //
       },
       3,
       {"ab+d", "exec", "abdd"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(RegExpLiteralsWide) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int wide_idx = 0;

  ExpectedSnippet<InstanceType, 257> snippets[] = {
      {"var a;" REPEAT_256(SPACE, "a = 1.23;") "return /ab+d/;",
       1 * kPointerSize,
       1,
       1031,
       {
           REPEAT_256(COMMA,                                     //
             B(LdaConstant), U8(wide_idx++),                     //
             B(Star), R(0)),                                     //
           B(CreateRegExpLiteralWide), U16(256), U16(0), U8(0),  //
           B(Return)                                             //
       },
       257,
       {REPEAT_256(COMMA, InstanceType::HEAP_NUMBER_TYPE),
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ArrayLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddKeyedStoreICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddKeyedStoreICSlot();
  FeedbackVectorSlot slot3 = feedback_spec.AddKeyedStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  int simple_flags =
      ArrayLiteral::kDisableMementos | ArrayLiteral::kShallowElements;
  int deep_elements_flags = ArrayLiteral::kDisableMementos;
  ExpectedSnippet<InstanceType> snippets[] = {
      {"return [ 1, 2 ];",
       0,
       1,
       5,
       {
           B(CreateArrayLiteral), U8(0), U8(0), U8(simple_flags),  //
           B(Return)                                               //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1; return [ a, a + 1 ];",
       4 * kPointerSize,
       1,
       38,
       {
           B(LdaSmi8), U8(1),                                               //
           B(Star), R(0),                                                   //
           B(CreateArrayLiteral), U8(0), U8(0), U8(3),                      //
           B(Star), R(2),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(1),                                                   //
           B(Ldar), R(0),                                                   //
           B(KeyedStoreICSloppy), R(2), R(1), U8(vector->GetIndex(slot1)),  //
           B(LdaSmi8), U8(1),                                               //
           B(Star), R(1),                                                   //
           B(Ldar), R(0),                                                   //
           B(Star), R(3),                                                   //
           B(LdaSmi8), U8(1),                                               //
           B(Add), R(3),                                                    //
           B(KeyedStoreICSloppy), R(2), R(1), U8(vector->GetIndex(slot1)),  //
           B(Ldar), R(2),                                                   //
           B(Return),                                                       //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"return [ [ 1, 2 ], [ 3 ] ];",
       0,
       1,
       5,
       {
           B(CreateArrayLiteral), U8(0), U8(2), U8(deep_elements_flags),  //
           B(Return)                                                      //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1; return [ [ a, 2 ], [ a + 2 ] ];",
       6 * kPointerSize,
       1,
       68,
       {
           B(LdaSmi8), U8(1),                                               //
           B(Star), R(0),                                                   //
           B(CreateArrayLiteral), U8(0), U8(2), U8(deep_elements_flags),    //
           B(Star), R(2),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(1),                                                   //
           B(CreateArrayLiteral), U8(1), U8(0), U8(simple_flags),           //
           B(Star), R(4),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(3),                                                   //
           B(Ldar), R(0),                                                   //
           B(KeyedStoreICSloppy), R(4), R(3), U8(vector->GetIndex(slot1)),  //
           B(Ldar), R(4),                                                   //
           B(KeyedStoreICSloppy), R(2), R(1), U8(vector->GetIndex(slot3)),  //
           B(LdaSmi8), U8(1),                                               //
           B(Star), R(1),                                                   //
           B(CreateArrayLiteral), U8(2), U8(1), U8(simple_flags),           //
           B(Star), R(4),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(3),                                                   //
           B(Ldar), R(0),                                                   //
           B(Star), R(5),                                                   //
           B(LdaSmi8), U8(2),                                               //
           B(Add), R(5),                                                    //
           B(KeyedStoreICSloppy), R(4), R(3), U8(vector->GetIndex(slot2)),  //
           B(Ldar), R(4),                                                   //
           B(KeyedStoreICSloppy), R(2), R(1), U8(vector->GetIndex(slot3)),  //
           B(Ldar), R(2),                                                   //
           B(Return),                                                       //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE, InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::FIXED_ARRAY_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ArrayLiteralsWide) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int wide_idx = 0;
  int simple_flags =
      ArrayLiteral::kDisableMementos | ArrayLiteral::kShallowElements;

  ExpectedSnippet<InstanceType, 257> snippets[] = {
      {"var a;" REPEAT_256(SPACE, "a = 1.23;") "return [ 1 , 2 ];",
       1 * kPointerSize,
       1,
       1031,
       {
           REPEAT_256(COMMA,                                               //
             B(LdaConstant), U8(wide_idx++),                               //
             B(Star), R(0)),                                               //
           B(CreateArrayLiteralWide), U16(256), U16(0), U8(simple_flags),  //
           B(Return)                                                       //
       },
       257,
       {REPEAT_256(COMMA, InstanceType::HEAP_NUMBER_TYPE),
        InstanceType::FIXED_ARRAY_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ObjectLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  int simple_flags = ObjectLiteral::kFastElements |
                     ObjectLiteral::kShallowProperties |
                     ObjectLiteral::kDisableMementos;
  int deep_elements_flags =
      ObjectLiteral::kFastElements | ObjectLiteral::kDisableMementos;
  ExpectedSnippet<InstanceType> snippets[] = {
      {"return { };",
       0,
       1,
       5,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(simple_flags),  //
           B(Return)                                                //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"return { name: 'string', val: 9.2 };",
       0,
       1,
       5,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Return)                                                       //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1; return { name: 'string', val: a };",
       2 * kPointerSize,
       1,
       19,
       {
           B(LdaSmi8), U8(1),                                              //
           B(Star), R(0),                                                  //
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Star), R(1),                                                  //
           B(Ldar), R(0),                                                  //
           B(StoreICSloppy), R(1), U8(1), U8(vector->GetIndex(slot1)),     //
           B(Ldar), R(1),                                                  //
           B(Return),                                                      //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var a = 1; return { val: a, val: a + 1 };",
       3 * kPointerSize,
       1,
       25,
       {
           B(LdaSmi8), U8(1),                                              //
           B(Star), R(0),                                                  //
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Star), R(1),                                                  //
           B(Ldar), R(0),                                                  //
           B(Star), R(2),                                                  //
           B(LdaSmi8), U8(1),                                              //
           B(Add), R(2),                                                   //
           B(StoreICSloppy), R(1), U8(1), U8(vector->GetIndex(slot1)),     //
           B(Ldar), R(1),                                                  //
           B(Return),                                                      //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"return { func: function() { } };",
       1 * kPointerSize,
       1,
       16,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Star), R(0),                                                  //
           B(CreateClosure), U8(1), U8(0),                                 //
           B(StoreICSloppy), R(0), U8(2), U8(vector->GetIndex(slot1)),     //
           B(Ldar), R(0),                                                  //
           B(Return),                                                      //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"return { func(a) { return a; } };",
       1 * kPointerSize,
       1,
       16,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Star), R(0),                                                  //
           B(CreateClosure), U8(1), U8(0),                                 //
           B(StoreICSloppy), R(0), U8(2), U8(vector->GetIndex(slot1)),     //
           B(Ldar), R(0),                                                  //
           B(Return),                                                      //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"return { get a() { return 2; } };",
       5 * kPointerSize,
       1,
       29,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),   //
           B(Star), R(0),                                                   //
           B(LdaConstant), U8(1),                                           //
           B(Star), R(1),                                                   //
           B(CreateClosure), U8(2), U8(0),                                  //
           B(Star), R(2),                                                   //
           B(LdaNull),                                                      //
           B(Star), R(3),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(4),                                                   //
           B(CallRuntime), U16(Runtime::kDefineAccessorPropertyUnchecked),  //
                           R(0), U8(5),                                     //
           B(Ldar), R(0),                                                   //
           B(Return),                                                       //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return { get a() { return this.x; }, set a(val) { this.x = val } };",
       5 * kPointerSize,
       1,
       31,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),   //
           B(Star), R(0),                                                   //
           B(LdaConstant), U8(1),                                           //
           B(Star), R(1),                                                   //
           B(CreateClosure), U8(2), U8(0),                                  //
           B(Star), R(2),                                                   //
           B(CreateClosure), U8(3), U8(0),                                  //
           B(Star), R(3),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(4),                                                   //
           B(CallRuntime), U16(Runtime::kDefineAccessorPropertyUnchecked),  //
                           R(0), U8(5),                                     //
           B(Ldar), R(0),                                                   //
           B(Return),                                                       //
       },
       4,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return { set b(val) { this.y = val } };",
       5 * kPointerSize,
       1,
       29,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),   //
           B(Star), R(0),                                                   //
           B(LdaConstant), U8(1),                                           //
           B(Star), R(1),                                                   //
           B(LdaNull),                                                      //
           B(Star), R(2),                                                   //
           B(CreateClosure), U8(2), U8(0),                                  //
           B(Star), R(3),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(4),                                                   //
           B(CallRuntime), U16(Runtime::kDefineAccessorPropertyUnchecked),  //
                           R(0), U8(5),                                     //
           B(Ldar), R(0),                                                   //
           B(Return),                                                       //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var a = 1; return { 1: a };",
       5 * kPointerSize,
       1,
       29,
       {
           B(LdaSmi8), U8(1),                                              //
           B(Star), R(0),                                                  //
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Star), R(1),                                                  //
           B(LdaSmi8), U8(1),                                              //
           B(Star), R(2),                                                  //
           B(Ldar), R(0),                                                  //
           B(Star), R(3),                                                  //
           B(LdaZero),                                                     //
           B(Star), R(4),                                                  //
           B(CallRuntime), U16(Runtime::kSetProperty), R(1), U8(4),        //
           B(Ldar), R(1),                                                  //
           B(Return),                                                      //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"return { __proto__: null }",
       2 * kPointerSize,
       1,
       17,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(simple_flags),            //
           B(Star), R(0),                                                     //
           B(LdaNull), B(Star), R(1),                                         //
           B(CallRuntime), U16(Runtime::kInternalSetPrototype), R(0), U8(2),  //
           B(Ldar), R(0),                                                     //
           B(Return),                                                         //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 'test'; return { [a]: 1 }",
       5 * kPointerSize,
       1,
       30,
       {
           B(LdaConstant), U8(0),                                             //
           B(Star), R(0),                                                     //
           B(CreateObjectLiteral), U8(1), U8(0), U8(simple_flags),            //
           B(Star), R(1),                                                     //
           B(Ldar), R(0),                                                     //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaSmi8), U8(1),                                                 //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineDataPropertyUnchecked), R(1),  //
                           U8(4),                                             //
           B(Ldar), R(1),                                                     //
           B(Return),                                                         //
       },
       2,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 'test'; return { val: a, [a]: 1 }",
       5 * kPointerSize,
       1,
       36,
       {
           B(LdaConstant), U8(0),                                             //
           B(Star), R(0),                                                     //
           B(CreateObjectLiteral), U8(1), U8(0), U8(deep_elements_flags),     //
           B(Star), R(1),                                                     //
           B(Ldar), R(0),                                                     //
           B(StoreICSloppy), R(1), U8(2), U8(vector->GetIndex(slot1)),        //
           B(Ldar), R(0),                                                     //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaSmi8), U8(1),                                                 //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineDataPropertyUnchecked), R(1),  //
                           U8(4),                                             //
           B(Ldar), R(1),                                                     //
           B(Return),                                                         //
       },
       3,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var a = 'test'; return { [a]: 1, __proto__: {} }",
       5 * kPointerSize,
       1,
       41,
       {
           B(LdaConstant), U8(0),                                             //
           B(Star), R(0),                                                     //
           B(CreateObjectLiteral), U8(1), U8(1), U8(simple_flags),            //
           B(Star), R(1),                                                     //
           B(Ldar), R(0),                                                     //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaSmi8), U8(1),                                                 //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineDataPropertyUnchecked), R(1),  //
                           U8(4),                                             //
           B(CreateObjectLiteral), U8(1), U8(0), U8(13),                      //
           B(Star), R(2),                                                     //
           B(CallRuntime), U16(Runtime::kInternalSetPrototype), R(1), U8(2),  //
           B(Ldar), R(1),                                                     //
           B(Return),                                                         //
       },
       2,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE}},
      {"var n = 'name'; return { [n]: 'val', get a() { }, set a(b) {} };",
       5 * kPointerSize,
       1,
       64,
       {
           B(LdaConstant), U8(0),                                             //
           B(Star), R(0),                                                     //
           B(CreateObjectLiteral), U8(1), U8(0), U8(simple_flags),            //
           B(Star), R(1),                                                     //
           B(Ldar), R(0),                                                     //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaConstant), U8(2),                                             //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineDataPropertyUnchecked), R(1),  //
                           U8(4),                                             //
           B(LdaConstant), U8(3),                                             //
           B(Star), R(2),                                                     //
           B(CreateClosure), U8(4), U8(0),                                    //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineGetterPropertyUnchecked),      //
                           R(1), U8(4),                                       //
           B(LdaConstant), U8(3),                                             //
           B(Star), R(2),                                                     //
           B(CreateClosure), U8(5), U8(0),                                    //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineSetterPropertyUnchecked),      //
                           R(1), U8(4),                                       //
           B(Ldar), R(1),                                                     //
           B(Return),                                                         //
       },
       6,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ObjectLiteralsWide) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int deep_elements_flags =
      ObjectLiteral::kFastElements | ObjectLiteral::kDisableMementos;
  int wide_idx = 0;

  ExpectedSnippet<InstanceType, 257> snippets[] = {
      {"var a;" REPEAT_256(SPACE,
                           "a = 1.23;") "return { name: 'string', val: 9.2 };",
       1 * kPointerSize,
       1,
       1031,
       {
           REPEAT_256(COMMA,                                     //
             B(LdaConstant), U8(wide_idx++),                     //
             B(Star), R(0)),                                     //
           B(CreateObjectLiteralWide), U16(256), U16(0),         //
                                       U8(deep_elements_flags),  //
           B(Return)                                             //
       },
       257,
       {REPEAT_256(COMMA, InstanceType::HEAP_NUMBER_TYPE),
        InstanceType::FIXED_ARRAY_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(TopLevelObjectLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int has_function_flags = ObjectLiteral::kFastElements |
                           ObjectLiteral::kHasFunction |
                           ObjectLiteral::kDisableMementos;
  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a = { func: function() { } };",
       5 * kPointerSize,
       1,
       48,
       {
           B(LdaConstant), U8(0),                                            //
           B(Star), R(1),                                                    //
           B(LdaZero),                                                       //
           B(Star), R(2),                                                    //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(1), U8(2),       //
           B(LdaConstant), U8(1),                                            //
           B(Star), R(1),                                                    //
           B(LdaZero),                                                       //
           B(Star), R(2),                                                    //
           B(CreateObjectLiteral), U8(2), U8(0), U8(has_function_flags),     //
           B(Star), R(4),                                                    //
           B(CreateClosure), U8(3), U8(1),                                   //
           B(StoreICSloppy), R(4), U8(4), U8(3),                             //
           B(CallRuntime), U16(Runtime::kToFastProperties), R(4), U8(1),     //
           B(Ldar), R(4),                                                    //
           B(Star), R(3),                                                    //
           B(CallRuntime), U16(Runtime::kInitializeVarGlobal), R(1), U8(3),  //
           B(LdaUndefined),                                                  //
           B(Return),                                                        //
       },
       5,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeTopLevelBytecode(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(TryCatch) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  // TODO(rmcilroy): modify tests when we have real try catch support.
  ExpectedSnippet<int> snippets[] = {
      {"try { return 1; } catch(e) { return 2; }",
       kPointerSize,
       1,
       3,
       {
           B(LdaSmi8), U8(1),  //
           B(Return),          //
       },
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(TryFinally) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  // TODO(rmcilroy): modify tests when we have real try finally support.
  ExpectedSnippet<int> snippets[] = {
      {"var a = 1; try { a = 2; } finally { a = 3; }",
       kPointerSize,
       1,
       14,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(LdaSmi8), U8(2),  //
           B(Star), R(0),      //
           B(LdaSmi8), U8(3),  //
           B(Star), R(0),      //
           B(LdaUndefined),    //
           B(Return),          //
       },
       0},
      {"var a = 1; try { a = 2; } catch(e) { a = 20 } finally { a = 3; }",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(LdaSmi8), U8(2),  //
           B(Star), R(0),      //
           B(LdaSmi8), U8(3),  //
           B(Star), R(0),      //
           B(LdaUndefined),    //
           B(Return),          //
       },
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(Throw) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  // TODO(rmcilroy): modify tests when we have real try catch support.
  ExpectedSnippet<const char*> snippets[] = {
      {"throw 1;",
       0,
       1,
       3,
       {
           B(LdaSmi8), U8(1),  //
           B(Throw),           //
       },
       0},
      {"throw 'Error';",
       0,
       1,
       3,
       {
           B(LdaConstant), U8(0),  //
           B(Throw),               //
       },
       1,
       {"Error"}},
      {"var a = 1; if (a) { throw 'Error'; };",
       1 * kPointerSize,
       1,
       11,
       {
           B(LdaSmi8), U8(1),               //
           B(Star), R(0),                   //
           B(JumpIfToBooleanFalse), U8(5),  //
           B(LdaConstant), U8(0),           //
           B(Throw),                        //
           B(LdaUndefined),                 //
           B(Return),                       //
       },
       1,
       {"Error"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(CallNew) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddGeneralSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddLoadICSlot();
  USE(slot1);

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<InstanceType> snippets[] = {
      {"function bar() { this.value = 0; }\n"
       "function f() { return new bar(); }\n"
       "f()",
       1 * kPointerSize,
       1,
       10,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                           //
           B(New), R(0), R(0), U8(0),                               //
           B(Return),                                               //
       },
       1,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"function bar(x) { this.value = 18; this.x = x;}\n"
       "function f() { return new bar(3); }\n"
       "f()",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                           //
           B(LdaSmi8), U8(3),                                       //
           B(Star), R(1),                                           //
           B(New), R(0), R(1), U8(1),                               //
           B(Return),                                               //
       },
       1,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"function bar(w, x, y, z) {\n"
       "  this.value = 18;\n"
       "  this.x = x;\n"
       "  this.y = y;\n"
       "  this.z = z;\n"
       "}\n"
       "function f() { return new bar(3, 4, 5); }\n"
       "f()",
       4 * kPointerSize,
       1,
       22,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                           //
           B(LdaSmi8), U8(3),                                       //
           B(Star), R(1),                                           //
           B(LdaSmi8), U8(4),                                       //
           B(Star), R(2),                                           //
           B(LdaSmi8), U8(5),                                       //
           B(Star), R(3),                                           //
           B(New), R(0), R(1), U8(3),                               //
           B(Return),                                               //
       },
       1,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ContextVariables) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot = feedback_spec.AddCallICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  int closure = Register::function_closure().index();
  int new_target = Register::new_target().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;

  // The wide check below relies on MIN_CONTEXT_SLOTS + 3 + 249 == 256, if this
  // ever changes, the REPEAT_XXX should be changed to output the correct number
  // of unique variables to trigger the wide slot load / store.
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS + 3 + 249 == 256);
  int wide_slot = first_context_slot + 3;

  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a; return function() { a = 1; };",
       1 * kPointerSize,
       1,
       11,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(0),                               //
           B(CreateClosure), U8(0), U8(0),                     //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var a = 1; return function() { a = 2; };",
       1 * kPointerSize,
       1,
       16,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(0),                               //
           B(LdaSmi8), U8(1),                                  //
           B(StaContextSlot), R(0), U8(first_context_slot),    //
           B(CreateClosure), U8(0), U8(0),                     //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var a = 1; var b = 2; return function() { a = 2; b = 3 };",
       1 * kPointerSize,
       1,
       21,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),    //
                           R(closure), U8(1),                    //
           B(PushContext), R(0),                                 //
           B(LdaSmi8), U8(1),                                    //
           B(StaContextSlot), R(0), U8(first_context_slot),      //
           B(LdaSmi8), U8(2),                                    //
           B(StaContextSlot), R(0), U8(first_context_slot + 1),  //
           B(CreateClosure), U8(0), U8(0),                       //
           B(Return),                                            //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var a; (function() { a = 2; })(); return a;",
       3 * kPointerSize,
       1,
       24,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),       //
                           R(closure), U8(1),                       //
           B(PushContext), R(0),                                    //
           B(LdaUndefined),                                         //
           B(Star), R(2),                                           //
           B(CreateClosure), U8(0), U8(0),                          //
           B(Star), R(1),                                           //
           B(Call), R(1), R(2), U8(0), U8(vector->GetIndex(slot)),  //
           B(LdaContextSlot), R(0), U8(first_context_slot),         //
           B(Return),                                               //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"'use strict'; let a = 1; { let b = 2; return function() { a + b; }; }",
       4 * kPointerSize,
       1,
       44,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),             //
                           R(closure), U8(1),                             //
           B(PushContext), R(0),                                          //
           B(LdaTheHole),                                                 //
           B(StaContextSlot), R(0), U8(first_context_slot),               //
           B(LdaSmi8), U8(1),                                             //
           B(StaContextSlot), R(0), U8(first_context_slot),               //
           B(LdaConstant), U8(0),                                         //
           B(Star), R(2),                                                 //
           B(Ldar), R(closure),                                           //
           B(Star), R(3),                                                 //
           B(CallRuntime), U16(Runtime::kPushBlockContext), R(2), U8(2),  //
           B(PushContext), R(1),                                          //
           B(LdaTheHole),                                                 //
           B(StaContextSlot), R(1), U8(first_context_slot),               //
           B(LdaSmi8), U8(2),                                             //
           B(StaContextSlot), R(1), U8(first_context_slot),               //
           B(CreateClosure), U8(1), U8(0),                                //
           B(Return),                                                     //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"'use strict';\n"
       REPEAT_249_UNIQUE_VARS()
       "eval();"
       "var b = 100;"
       "return b",
       3 * kPointerSize,
       1,
       1041,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),  //
                           U8(1),                                          //
           B(PushContext), R(0),                                           //
           B(Ldar), THIS(1),                                               //
           B(StaContextSlot), R(0), U8(first_context_slot),                //
           B(CreateUnmappedArguments),                                     //
           B(StaContextSlot), R(0), U8(first_context_slot + 1),            //
           B(Ldar), R(new_target),                                         //
           B(StaContextSlot), R(0), U8(first_context_slot + 2),            //
           REPEAT_249(COMMA,                                               //
                      B(LdaZero),                                          //
                      B(StaContextSlot), R(0), U8(wide_slot++)),           //
           B(LdaUndefined),                                                //
           B(Star), R(2),                                                  //
           B(LdaGlobalStrict), U8(0), U8(1),                               //
           B(Star), R(1),                                                  //
           B(Call), R(1), R(2), U8(0), U8(0),                              //
           B(LdaSmi8), U8(100),                                            //
           B(StaContextSlotWide), R(0), U16(256),                          //
           B(LdaContextSlotWide), R(0), U16(256),                          //
           B(Return),                                                      //
       },
       1,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ContextParameters) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int closure = Register::function_closure().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;

  ExpectedSnippet<InstanceType> snippets[] = {
      {"function f(arg1) { return function() { arg1 = 2; }; }",
       1 * kPointerSize,
       2,
       16,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(0),                               //
           B(Ldar), R(helper.kLastParamIndex),                 //
           B(StaContextSlot), R(0), U8(first_context_slot),    //
           B(CreateClosure), U8(0), U8(0),                     //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"function f(arg1) { var a = function() { arg1 = 2; }; return arg1; }",
       2 * kPointerSize,
       2,
       21,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(1),                               //
           B(Ldar), R(helper.kLastParamIndex),                 //
           B(StaContextSlot), R(1), U8(first_context_slot),    //
           B(CreateClosure), U8(0), U8(0),                     //
           B(Star), R(0),                                      //
           B(LdaContextSlot), R(1), U8(first_context_slot),    //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"function f(a1, a2, a3, a4) { return function() { a1 = a3; }; }",
       1 * kPointerSize,
       5,
       21,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),    //
                           R(closure), U8(1),                    //
           B(PushContext), R(0),                                 //
           B(Ldar), R(helper.kLastParamIndex - 3),               //
           B(StaContextSlot), R(0), U8(first_context_slot + 1),  //
           B(Ldar), R(helper.kLastParamIndex -1),                //
           B(StaContextSlot), R(0), U8(first_context_slot),      //
           B(CreateClosure), U8(0), U8(0),                       //
           B(Return),                                            //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"function f() { var self = this; return function() { self = 2; }; }",
       1 * kPointerSize,
       1,
       16,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(0),                               //
           B(Ldar), R(helper.kLastParamIndex),                 //
           B(StaContextSlot), R(0), U8(first_context_slot),    //
           B(CreateClosure), U8(0), U8(0),                     //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunction(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(OuterContextVariables) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int context = Register::function_context().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;

  ExpectedSnippet<InstanceType> snippets[] = {
      {"function Outer() {"
       "  var outerVar = 1;"
       "  function Inner(innerArg) {"
       "    this.innerFunc = function() { return outerVar * innerArg; }"
       "  }"
       "  this.getInnerFunc = function() { return new Inner(1).innerFunc; }"
       "}"
       "var f = new Outer().getInnerFunc();",
       2 * kPointerSize,
       1,
       20,
       {
           B(Ldar), R(context),                                    //
           B(Star), R(0),                                          //
           B(LdaContextSlot), R(0), U8(Context::PREVIOUS_INDEX),   //
           B(Star), R(0),                                          //
           B(LdaContextSlot), R(0), U8(first_context_slot),        //
           B(Star), R(1),                                          //
           B(LdaContextSlot), R(context), U8(first_context_slot),  //
           B(Mul), R(1),                                           //
           B(Return),                                              //
       }},
      {"function Outer() {"
       "  var outerVar = 1;"
       "  function Inner(innerArg) {"
       "    this.innerFunc = function() { outerVar = innerArg; }"
       "  }"
       "  this.getInnerFunc = function() { return new Inner(1).innerFunc; }"
       "}"
       "var f = new Outer().getInnerFunc();",
       2 * kPointerSize,
       1,
       21,
       {
           B(LdaContextSlot), R(context), U8(first_context_slot),  //
           B(Star), R(0),                                          //
           B(Ldar), R(context),                                    //
           B(Star), R(1),                                          //
           B(LdaContextSlot), R(1), U8(Context::PREVIOUS_INDEX),   //
           B(Star), R(1),                                          //
           B(Ldar), R(0),                                          //
           B(StaContextSlot), R(1), U8(first_context_slot),        //
           B(LdaUndefined),                                        //
           B(Return),                                              //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionNoFilter(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(CountOperators) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddLoadICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddStoreICSlot();
  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  FeedbackVectorSpec store_feedback_spec(&zone);
  FeedbackVectorSlot store_slot = store_feedback_spec.AddStoreICSlot();
  Handle<i::TypeFeedbackVector> store_vector =
      i::NewTypeFeedbackVector(helper.isolate(), &store_feedback_spec);

  int closure = Register::function_closure().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;

  int object_literal_flags =
      ObjectLiteral::kFastElements | ObjectLiteral::kDisableMementos;
  int array_literal_flags =
      ArrayLiteral::kDisableMementos | ArrayLiteral::kShallowElements;

  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a = 1; return ++a;",
       1 * kPointerSize,
       1,
       9,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(ToNumber),        //
           B(Inc),             //
           B(Star), R(0),      //
           B(Return),          //
       }},
      {"var a = 1; return a++;",
       2 * kPointerSize,
       1,
       13,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(ToNumber),        //
           B(Star), R(1),      //
           B(Inc),             //
           B(Star), R(0),      //
           B(Ldar), R(1),      //
           B(Return),          //
       }},
      {"var a = 1; return --a;",
       1 * kPointerSize,
       1,
       9,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(ToNumber),        //
           B(Dec),             //
           B(Star), R(0),      //
           B(Return),          //
       }},
      {"var a = 1; return a--;",
       2 * kPointerSize,
       1,
       13,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(ToNumber),        //
           B(Star), R(1),      //
           B(Dec),             //
           B(Star), R(0),      //
           B(Ldar), R(1),      //
           B(Return),          //
       }},
      {"var a = { val: 1 }; return a.val++;",
       3 * kPointerSize,
       1,
       23,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(object_literal_flags),  //
           B(Star), R(0),                                                   //
           B(Star), R(1),                                                   //
           B(LoadICSloppy), R(1), U8(1), U8(vector->GetIndex(slot1)),       //
           B(ToNumber),                                                     //
           B(Star), R(2),                                                   //
           B(Inc),                                                          //
           B(StoreICSloppy), R(1), U8(1), U8(vector->GetIndex(slot2)),      //
           B(Ldar), R(2),                                                   //
           B(Return),                                                       //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var a = { val: 1 }; return --a.val;",
       2 * kPointerSize,
       1,
       19,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(object_literal_flags),  //
           B(Star), R(0),                                                   //
           B(Star), R(1),                                                   //
           B(LoadICSloppy), R(1), U8(1), U8(vector->GetIndex(slot1)),       //
           B(ToNumber),                                                     //
           B(Dec),                                                          //
           B(StoreICSloppy), R(1), U8(1), U8(vector->GetIndex(slot2)),      //
           B(Return),                                                       //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var name = 'var'; var a = { val: 1 }; return a[name]--;",
       5 * kPointerSize,
       1,
       30,
       {
           B(LdaConstant), U8(0),                                           //
           B(Star), R(0),                                                   //
           B(CreateObjectLiteral), U8(1), U8(0), U8(object_literal_flags),  //
           B(Star), R(1),                                                   //
           B(Star), R(2),                                                   //
           B(Ldar), R(0),                                                   //
           B(Star), R(3),                                                   //
           B(KeyedLoadICSloppy), R(2), U8(vector->GetIndex(slot1)),         //
           B(ToNumber),                                                     //
           B(Star), R(4),                                                   //
           B(Dec),                                                          //
           B(KeyedStoreICSloppy), R(2), R(3), U8(vector->GetIndex(slot2)),  //
           B(Ldar), R(4),                                                   //
           B(Return),                                                       //
       },
       2,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE}},
      {"var name = 'var'; var a = { val: 1 }; return ++a[name];",
       4 * kPointerSize,
       1,
       26,
       {
           B(LdaConstant), U8(0),                                           //
           B(Star), R(0),                                                   //
           B(CreateObjectLiteral), U8(1), U8(0), U8(object_literal_flags),  //
           B(Star), R(1),                                                   //
           B(Star), R(2),                                                   //
           B(Ldar), R(0),                                                   //
           B(Star), R(3),                                                   //
           B(KeyedLoadICSloppy), R(2), U8(vector->GetIndex(slot1)),         //
           B(ToNumber),                                                     //
           B(Inc),                                                          //
           B(KeyedStoreICSloppy), R(2), R(3), U8(vector->GetIndex(slot2)),  //
           B(Return),                                                       //
       },
       2,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1; var b = function() { return a }; return ++a;",
       2 * kPointerSize,
       1,
       26,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),  //
                           U8(1),                                          //
           B(PushContext), R(1),                                           //
           B(LdaSmi8), U8(1),                                              //
           B(StaContextSlot), R(1), U8(first_context_slot),                //
           B(CreateClosure), U8(0), U8(0),                                 //
           B(Star), R(0),                                                  //
           B(LdaContextSlot), R(1), U8(first_context_slot),                //
           B(ToNumber),                                                    //
           B(Inc),                                                         //
           B(StaContextSlot), R(1), U8(first_context_slot),                //
           B(Return),                                                      //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var a = 1; var b = function() { return a }; return a--;",
       3 * kPointerSize,
       1,
       30,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),  //
                           U8(1),                                          //
           B(PushContext), R(1),                                           //
           B(LdaSmi8), U8(1),                                              //
           B(StaContextSlot), R(1), U8(first_context_slot),                //
           B(CreateClosure), U8(0), U8(0),                                 //
           B(Star), R(0),                                                  //
           B(LdaContextSlot), R(1), U8(first_context_slot),                //
           B(ToNumber),                                                    //
           B(Star), R(2),                                                  //
           B(Dec),                                                         //
           B(StaContextSlot), R(1), U8(first_context_slot),                //
           B(Ldar), R(2),                                                  //
           B(Return),                                                      //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var idx = 1; var a = [1, 2]; return a[idx++] = 2;",
       4 * kPointerSize,
       1,
       27,
       {
           B(LdaSmi8), U8(1),                                              //
           B(Star), R(0),                                                  //
           B(CreateArrayLiteral), U8(0), U8(0), U8(array_literal_flags),   //
           B(Star), R(1),                                                  //
           B(Star), R(2),                                                  //
           B(Ldar), R(0),                                                  //
           B(ToNumber),                                                    //
           B(Star), R(3),                                                  //
           B(Inc),                                                         //
           B(Star), R(0),                                                  //
           B(LdaSmi8), U8(2),                                              //
           B(KeyedStoreICSloppy), R(2), R(3),                              //
                                  U8(store_vector->GetIndex(store_slot)),  //
           B(Return),                                                      //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(GlobalCountOperators) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddLoadICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"var global = 1;\nfunction f() { return ++global; }\nf()",
       0,
       1,
       9,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot1)),  //
           B(ToNumber),                                             //
           B(Inc),                                                  //
           B(StaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Return),                                               //
       },
       1,
       {"global"}},
      {"var global = 1;\nfunction f() { return global--; }\nf()",
       1 * kPointerSize,
       1,
       13,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot1)),  //
           B(ToNumber),                                             //
           B(Star), R(0),                                           //
           B(Dec),                                                  //
           B(StaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Ldar), R(0),                                           //
           B(Return),
       },
       1,
       {"global"}},
      {"unallocated = 1;\nfunction f() { 'use strict'; return --unallocated; }"
       "f()",
       0,
       1,
       9,
       {
           B(LdaGlobalStrict), U8(0), U8(vector->GetIndex(slot1)),  //
           B(ToNumber),                                             //
           B(Dec),                                                  //
           B(StaGlobalStrict), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Return),                                               //
       },
       1,
       {"unallocated"}},
      {"unallocated = 1;\nfunction f() { return unallocated++; }\nf()",
       1 * kPointerSize,
       1,
       13,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot1)),  //
           B(ToNumber),                                             //
           B(Star), R(0),                                           //
           B(Inc),                                                  //
           B(StaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Ldar), R(0),                                           //
           B(Return),
       },
       1,
       {"unallocated"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(CompoundExpressions) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int closure = Register::function_closure().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddLoadICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  int object_literal_flags =
      ObjectLiteral::kFastElements | ObjectLiteral::kDisableMementos;
  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a = 1; a += 2;",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(Star), R(1),      //
           B(LdaSmi8), U8(2),  //
           B(Add), R(1),       //
           B(Star), R(0),      //
           B(LdaUndefined),    //
           B(Return),          //
       }},
      {"var a = 1; a /= 2;",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(Star), R(1),      //
           B(LdaSmi8), U8(2),  //
           B(Div), R(1),       //
           B(Star), R(0),      //
           B(LdaUndefined),    //
           B(Return),          //
       }},
      {"var a = { val: 2 }; a.name *= 2;",
       3 * kPointerSize,
       1,
       24,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(object_literal_flags),  //
           B(Star), R(0),                                                   //
           B(Star), R(1),                                                   //
           B(LoadICSloppy), R(1), U8(1), U8(vector->GetIndex(slot1)),       //
           B(Star), R(2),                                                   //
           B(LdaSmi8), U8(2),                                               //
           B(Mul), R(2),                                                    //
           B(StoreICSloppy), R(1), U8(1), U8(vector->GetIndex(slot2)),      //
           B(LdaUndefined),                                                 //
           B(Return),                                                       //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var a = { 1: 2 }; a[1] ^= 2;",
       4 * kPointerSize,
       1,
       27,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(object_literal_flags),  //
           B(Star), R(0),                                                   //
           B(Star), R(1),                                                   //
           B(LdaSmi8), U8(1),                                               //
           B(Star), R(2),                                                   //
           B(KeyedLoadICSloppy), R(1), U8(vector->GetIndex(slot1)),         //
           B(Star), R(3),                                                   //
           B(LdaSmi8), U8(2),                                               //
           B(BitwiseXor), R(3),                                             //
           B(KeyedStoreICSloppy), R(1), R(2), U8(vector->GetIndex(slot2)),  //
           B(LdaUndefined),                                                 //
           B(Return),                                                       //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1; (function f() { return a; }); a |= 24;",
       2 * kPointerSize,
       1,
       29,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),  //
                           U8(1),                                          //
           B(PushContext), R(0),                                           //
           B(LdaSmi8), U8(1),                                              //
           B(StaContextSlot), R(0), U8(first_context_slot),                //
           B(CreateClosure), U8(0), U8(0),                                 //
           B(LdaContextSlot), R(0), U8(first_context_slot),                //
           B(Star), R(1),                                                  //
           B(LdaSmi8), U8(24),                                             //
           B(BitwiseOr), R(1),                                             //
           B(StaContextSlot), R(0), U8(first_context_slot),                //
           B(LdaUndefined),                                                //
           B(Return),                                                      //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(GlobalCompoundExpressions) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddLoadICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"var global = 1;\nfunction f() { return global &= 1; }\nf()",
       1 * kPointerSize,
       1,
       13,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot1)),  //
           B(Star), R(0),                                           //
           B(LdaSmi8), U8(1),                                       //
           B(BitwiseAnd), R(0),                                     //
           B(StaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Return),                                               //
       },
       1,
       {"global"}},
      {"unallocated = 1;\nfunction f() { return unallocated += 1; }\nf()",
       1 * kPointerSize,
       1,
       13,
       {
           B(LdaGlobalSloppy), U8(0), U8(vector->GetIndex(slot1)),  //
           B(Star), R(0),                                           //
           B(LdaSmi8), U8(1),                                       //
           B(Add), R(0),                                            //
           B(StaGlobalSloppy), U8(0), U8(vector->GetIndex(slot2)),  //
           B(Return),                                               //
       },
       1,
       {"unallocated"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(CreateArguments) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int closure = Register::function_closure().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot = feedback_spec.AddKeyedLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"function f() { return arguments; }",
       1 * kPointerSize,
       1,
       4,
       {
           B(CreateMappedArguments),  //
           B(Star), R(0),             //
           B(Return),                 //
       }},
      {"function f() { return arguments[0]; }",
       2 * kPointerSize,
       1,
       10,
       {
           B(CreateMappedArguments),                                //
           B(Star), R(0),                                           //
           B(Star), R(1),                                           //
           B(LdaZero),                                              //
           B(KeyedLoadICSloppy), R(1), U8(vector->GetIndex(slot)),  //
           B(Return),                                               //
       }},
      {"function f() { 'use strict'; return arguments; }",
       1 * kPointerSize,
       1,
       4,
       {
           B(CreateUnmappedArguments),  //
           B(Star), R(0),               //
           B(Return),                   //
       }},
      {"function f(a) { return arguments[0]; }",
       3 * kPointerSize,
       2,
       22,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),  //
                           U8(1),                                          //
           B(PushContext), R(1),                                           //
           B(Ldar), R(BytecodeGeneratorHelper::kLastParamIndex),           //
           B(StaContextSlot), R(1), U8(first_context_slot),                //
           B(CreateMappedArguments),                                       //
           B(Star), R(0),                                                  //
           B(Star), R(2),                                                  //
           B(LdaZero),                                                     //
           B(KeyedLoadICSloppy), R(2), U8(vector->GetIndex(slot)),         //
           B(Return),                                                      //
       }},
      {"function f(a, b, c) { return arguments; }",
       2 * kPointerSize,
       4,
       26,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),  //
                           U8(1),                                          //
           B(PushContext), R(1),                                           //
           B(Ldar), R(BytecodeGeneratorHelper::kLastParamIndex - 2),       //
           B(StaContextSlot), R(1), U8(first_context_slot + 2),            //
           B(Ldar), R(BytecodeGeneratorHelper::kLastParamIndex - 1),       //
           B(StaContextSlot), R(1), U8(first_context_slot + 1),            //
           B(Ldar), R(BytecodeGeneratorHelper::kLastParamIndex),           //
           B(StaContextSlot), R(1), U8(first_context_slot),                //
           B(CreateMappedArguments),                                       //
           B(Star), R(0),                                                  //
           B(Return),                                                      //
       }},
      {"function f(a, b, c) { 'use strict'; return arguments; }",
       1 * kPointerSize,
       4,
       4,
       {
           B(CreateUnmappedArguments),  //
           B(Star), R(0),               //
           B(Return),                   //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunction(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(IllegalRedeclaration) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  CHECK_GE(MessageTemplate::kVarRedeclaration, 128);
  // Must adapt bytecode if this changes.

  ExpectedSnippet<Handle<Object>, 2> snippets[] = {
      {"const a = 1; { var a = 2; }",
       3 * kPointerSize,
       1,
       14,
       {
           B(LdaConstant), U8(0),                                       //
           B(Star), R(1),                                               //
           B(LdaConstant), U8(1),                                       //
           B(Star), R(2),                                               //
           B(CallRuntime), U16(Runtime::kNewSyntaxError), R(1), U8(2),  //
           B(Throw),                                                    //
       },
       2,
       {helper.factory()->NewNumberFromInt(MessageTemplate::kVarRedeclaration),
        helper.factory()->NewStringFromAsciiChecked("a")}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ForIn) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int simple_flags =
      ArrayLiteral::kDisableMementos | ArrayLiteral::kShallowElements;
  int deep_elements_flags =
      ObjectLiteral::kFastElements | ObjectLiteral::kDisableMementos;

  FeedbackVectorSpec feedback_spec(&zone);
  feedback_spec.AddStoreICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddStoreICSlot();
  FeedbackVectorSlot slot3 = feedback_spec.AddStoreICSlot();
  FeedbackVectorSlot slot4 = feedback_spec.AddStoreICSlot();
  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<InstanceType> snippets[] = {
      {"for (var p in null) {}",
       2 * kPointerSize,
       1,
       2,
       {B(LdaUndefined), B(Return)},
       0},
      {"for (var p in undefined) {}",
       2 * kPointerSize,
       1,
       2,
       {B(LdaUndefined), B(Return)},
       0},
      {"for (var p in undefined) {}",
       2 * kPointerSize,
       1,
       2,
       {B(LdaUndefined), B(Return)},
       0},
      {"var x = 'potatoes';\n"
       "for (var p in x) { return p; }",
       8 * kPointerSize,
       1,
       45,
       {
           B(LdaConstant), U8(0),                 //
           B(Star), R(1),                         //
           B(JumpIfUndefined), U8(39),            //
           B(JumpIfNull), U8(37),                 //
           B(ToObject),                           //
           B(JumpIfNull), U8(34),                 //
           B(Star), R(3),                         //
           B(ForInPrepare), R(4), R(5), R(6),     //
           B(LdaZero),                            //
           B(Star), R(7),                         //
           B(ForInDone), R(7), R(6),              //
           B(JumpIfTrue), U8(20),                 //
           B(ForInNext), R(3), R(4), R(5), R(7),  //
           B(JumpIfUndefined), U8(7),             //
           B(Star), R(0),                         //
           B(Star), R(2),                         //
           B(Return),                             //
           B(ForInStep), R(7),                    //
           B(Star), R(7),                         //
           B(Jump), U8(-21),                      //
           B(LdaUndefined),                       //
           B(Return),                             //
       },
       1,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var x = 0;\n"
       "for (var p in [1,2,3]) { x += p; }",
       9 * kPointerSize,
       1,
       57,
       {
           B(LdaZero),                                  //
           B(Star), R(1),                               //
           B(CreateArrayLiteral), U8(0), U8(0), U8(3),  //
           B(JumpIfUndefined), U8(48),                  //
           B(JumpIfNull), U8(46),                       //
           B(ToObject),                                 //
           B(JumpIfNull), U8(43),                       //
           B(Star), R(3),                               //
           B(ForInPrepare), R(4), R(5), R(6),           //
           B(LdaZero),                                  //
           B(Star), R(7),                               //
           B(ForInDone), R(7), R(6),                    //
           B(JumpIfTrue), U8(29),                       //
           B(ForInNext), R(3), R(4), R(5), R(7),        //
           B(JumpIfUndefined), U8(16),                  //
           B(Star), R(0),                               //
           B(Star), R(2),                               //
           B(Ldar), R(1),                               //
           B(Star), R(8),                               //
           B(Ldar), R(2),                               //
           B(Add), R(8),                                //
           B(Star), R(1),                               //
           B(ForInStep), R(7),                          //
           B(Star), R(7),                               //
           B(Jump), U8(-30),                            //
           B(LdaUndefined),                             //
           B(Return),                                   //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var x = { 'a': 1, 'b': 2 };\n"
       "for (x['a'] in [10, 20, 30]) {\n"
       "  if (x['a'] == 10) continue;\n"
       "  if (x['a'] == 20) break;\n"
       "}",
       8 * kPointerSize,
       1,
       94,
       {
           B(CreateObjectLiteral), U8(0), U8(0), U8(deep_elements_flags),  //
           B(Star), R(0),                                                  //
           B(CreateArrayLiteral), U8(1), U8(1), U8(simple_flags),          //
           B(JumpIfUndefined), U8(82),                                     //
           B(JumpIfNull), U8(80),                                          //
           B(ToObject),                                                    //
           B(JumpIfNull), U8(77),                                          //
           B(Star), R(1),                                                  //
           B(ForInPrepare), R(2), R(3), R(4),                              //
           B(LdaZero),                                                     //
           B(Star), R(5),                                                  //
           B(ForInDone), R(5), R(4),                                       //
           B(JumpIfTrue), U8(63),                                          //
           B(ForInNext), R(1), R(2), R(3), R(5),                           //
           B(JumpIfUndefined), U8(50),                                     //
           B(Star), R(6),                                                  //
           B(Ldar), R(0),                                                  //
           B(Star), R(7),                                                  //
           B(Ldar), R(6),                                                  //
           B(StoreICSloppy), R(7), U8(2), U8(vector->GetIndex(slot4)),     //
           B(Ldar), R(0),                                                  //
           B(Star), R(6),                                                  //
           B(LoadICSloppy), R(6), U8(2), U8(vector->GetIndex(slot2)),      //
           B(Star), R(7),                                                  //
           B(LdaSmi8), U8(10),                                             //
           B(TestEqual), R(7),                                             //
           B(JumpIfFalse), U8(4),                                          //
           B(Jump), U8(20),                                                //
           B(Ldar), R(0),                                                  //
           B(Star), R(6),                                                  //
           B(LoadICSloppy), R(6), U8(2), U8(vector->GetIndex(slot3)),      //
           B(Star), R(7),                                                  //
           B(LdaSmi8), U8(20),                                             //
           B(TestEqual), R(7),                                             //
           B(JumpIfFalse), U8(4),                                          //
           B(Jump), U8(8),                                                 //
           B(ForInStep), R(5),                                             //
           B(Star), R(5),                                                  //
           B(Jump), U8(-64),                                               //
           B(LdaUndefined),                                                //
           B(Return),                                                      //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE, InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var x = [ 10, 11, 12 ] ;\n"
       "for (x[0] in [1,2,3]) { return x[3]; }",
       9 * kPointerSize,
       1,
       71,
       {
           B(CreateArrayLiteral), U8(0), U8(0), U8(simple_flags),           //
           B(Star), R(0),                                                   //
           B(CreateArrayLiteral), U8(1), U8(1), U8(simple_flags),           //
           B(JumpIfUndefined), U8(59),                                      //
           B(JumpIfNull), U8(57),                                           //
           B(ToObject),                                                     //
           B(JumpIfNull), U8(54),                                           //
           B(Star), R(1),                                                   //
           B(ForInPrepare), R(2), R(3), R(4),                               //
           B(LdaZero),                                                      //
           B(Star), R(5),                                                   //
           B(ForInDone), R(5), R(4),                                        //
           B(JumpIfTrue), U8(40),                                           //
           B(ForInNext), R(1), R(2), R(3), R(5),                            //
           B(JumpIfUndefined), U8(27),                                      //
           B(Star), R(6),                                                   //
           B(Ldar), R(0),                                                   //
           B(Star), R(7),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(8),                                                   //
           B(Ldar), R(6),                                                   //
           B(KeyedStoreICSloppy), R(7), R(8), U8(vector->GetIndex(slot3)),  //
           B(Ldar), R(0),                                                   //
           B(Star), R(6),                                                   //
           B(LdaSmi8), U8(3),                                               //
           B(KeyedLoadICSloppy), R(6), U8(vector->GetIndex(slot2)),         //
           B(Return),                                                       //
           B(ForInStep), R(5),                                              //
           B(Star), R(5),                                                   //
           B(Jump), U8(-41),                                                //
           B(LdaUndefined),                                                 //
           B(Return),                                                       //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE, InstanceType::FIXED_ARRAY_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(Conditional) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"return 1 ? 2 : 3;",
       0,
       1,
       11,
       {
           B(LdaSmi8), U8(1),               //
           B(JumpIfToBooleanFalse), U8(6),  //
           B(LdaSmi8), U8(2),               //
           B(Jump), U8(4),                  //
           B(LdaSmi8), U8(3),               //
           B(Return),                       //
       }},
      {"return 1 ? 2 ? 3 : 4 : 5;",
       0,
       1,
       19,
       {
           B(LdaSmi8), U8(1),                //
           B(JumpIfToBooleanFalse), U8(14),  //
           B(LdaSmi8), U8(2),                //
           B(JumpIfToBooleanFalse), U8(6),   //
           B(LdaSmi8), U8(3),                //
           B(Jump), U8(4),                   //
           B(LdaSmi8), U8(4),                //
           B(Jump), U8(4),                   //
           B(LdaSmi8), U8(5),                //
           B(Return),                        //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(Switch) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var a = 1;\n"
       "switch(a) {\n"
       " case 1: return 2;\n"
       " case 2: return 3;\n"
       "}\n",
       3 * kPointerSize,
       1,
       30,
       {
           B(LdaSmi8), U8(1),         //
           B(Star), R(1),             // The tag variable is allocated as a
           B(Star), R(0),             // local by the parser, hence the store
           B(Star), R(2),             // to another local register.
           B(LdaSmi8), U8(1),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(10),     //
           B(LdaSmi8), U8(2),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(7),      //
           B(Jump), U8(8),            //
           B(LdaSmi8), U8(2),         //
           B(Return),                 //
           B(LdaSmi8), U8(3),         //
           B(Return),                 //
           B(LdaUndefined),           //
           B(Return),                 //
       }},
      {"var a = 1;\n"
       "switch(a) {\n"
       " case 1: a = 2; break;\n"
       " case 2: a = 3; break;\n"
       "}\n",
       3 * kPointerSize,
       1,
       36,
       {
           B(LdaSmi8), U8(1),         //
           B(Star), R(1),             //
           B(Star), R(0),             //
           B(Star), R(2),             //
           B(LdaSmi8), U8(1),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(10),     //
           B(LdaSmi8), U8(2),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(10),     //
           B(Jump), U8(14),           //
           B(LdaSmi8), U8(2),         //
           B(Star), R(1),             //
           B(Jump), U8(8),            //
           B(LdaSmi8), U8(3),         //
           B(Star), R(1),             //
           B(Jump), U8(2),            //
           B(LdaUndefined),           //
           B(Return),                 //
       }},
      {"var a = 1;\n"
       "switch(a) {\n"
       " case 1: a = 2; // fall-through\n"
       " case 2: a = 3; break;\n"
       "}\n",
       3 * kPointerSize,
       1,
       34,
       {
           B(LdaSmi8), U8(1),         //
           B(Star), R(1),             //
           B(Star), R(0),             //
           B(Star), R(2),             //
           B(LdaSmi8), U8(1),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(10),     //
           B(LdaSmi8), U8(2),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(8),      //
           B(Jump), U8(12),           //
           B(LdaSmi8), U8(2),         //
           B(Star), R(1),             //
           B(LdaSmi8), U8(3),         //
           B(Star), R(1),             //
           B(Jump), U8(2),            //
           B(LdaUndefined),           //
           B(Return),                 //
       }},
      {"var a = 1;\n"
       "switch(a) {\n"
       " case 2: break;\n"
       " case 3: break;\n"
       " default: a = 1; break;\n"
       "}\n",
       3 * kPointerSize,
       1,
       34,
       {
           B(LdaSmi8), U8(1),         //
           B(Star), R(1),             //
           B(Star), R(0),             //
           B(Star), R(2),             //
           B(LdaSmi8), U8(2),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(10),     //
           B(LdaSmi8), U8(3),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(6),      //
           B(Jump), U8(6),            //
           B(Jump), U8(10),           //
           B(Jump), U8(8),            //
           B(LdaSmi8), U8(1),         //
           B(Star), R(1),             //
           B(Jump), U8(2),            //
           B(LdaUndefined),           //
           B(Return),                 //
       }},
      {"var a = 1;\n"
       "switch(typeof(a)) {\n"
       " case 2: a = 1; break;\n"
       " case 3: a = 2; break;\n"
       " default: a = 3; break;\n"
       "}\n",
       3 * kPointerSize,
       1,
       43,
       {
           B(LdaSmi8), U8(1),         //
           B(Star), R(1),             //
           B(TypeOf),                 //
           B(Star), R(0),             //
           B(Star), R(2),             //
           B(LdaSmi8), U8(2),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(10),     //
           B(LdaSmi8), U8(3),         //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(10),     //
           B(Jump), U8(14),           //
           B(LdaSmi8), U8(1),         //
           B(Star), R(1),             //
           B(Jump), U8(14),           //
           B(LdaSmi8), U8(2),         //
           B(Star), R(1),             //
           B(Jump), U8(8),            //
           B(LdaSmi8), U8(3),         //
           B(Star), R(1),             //
           B(Jump), U8(2),            //
           B(LdaUndefined),           //
           B(Return),                 //
       }},
      {"var a = 1;\n"
       "switch(a) {\n"
       " case typeof(a): a = 1; break;\n"
       " default: a = 2; break;\n"
       "}\n",
       3 * kPointerSize,
       1,
       31,
       {
           B(LdaSmi8), U8(1),         //
           B(Star), R(1),             //
           B(Star), R(0),             //
           B(Star), R(2),             //
           B(Ldar), R(1),             //
           B(TypeOf),                 //
           B(TestEqualStrict), R(2),  //
           B(JumpIfTrue), U8(4),      //
           B(Jump), U8(8),            //
           B(LdaSmi8), U8(1),         //
           B(Star), R(1),             //
           B(Jump), U8(8),            //
           B(LdaSmi8), U8(2),         //
           B(Star), R(1),             //
           B(Jump), U8(2),            //
           B(LdaUndefined),           //
           B(Return),                 //
       }},
      {"var a = 1;\n"
       "switch(a) {\n"
       " case 1:\n" REPEAT_64(SPACE, "  a = 2;")
       "break;\n"
       " case 2: a = 3; break;"
       "}\n",
       3 * kPointerSize,
       1,
       288,
       {
           B(LdaSmi8), U8(1),             //
           B(Star), R(1),                 //
           B(Star), R(0),                 //
           B(Star), R(2),                 //
           B(LdaSmi8), U8(1),             //
           B(TestEqualStrict), R(2),      //
           B(JumpIfTrue), U8(10),         //
           B(LdaSmi8), U8(2),             //
           B(TestEqualStrict), R(2),      //
           B(JumpIfTrueConstant), U8(0),  //
           B(JumpConstant), U8(1),        //
           REPEAT_64(COMMA,               //
                     B(LdaSmi8), U8(2),   //
                     B(Star), R(1)),      //
           B(Jump), U8(8),                //
           B(LdaSmi8), U8(3),             //
           B(Star), R(1),                 //
           B(Jump), U8(2),                //
           B(LdaUndefined),               //
           B(Return),                     //
       },
       2,
       {262, 266}},
      {"var a = 1;\n"
       "switch(a) {\n"
       " case 1: \n"
       "   switch(a + 1) {\n"
       "      case 2 : a = 1; break;\n"
       "      default : a = 2; break;\n"
       "   }  // fall-through\n"
       " case 2: a = 3;\n"
       "}\n",
       5 * kPointerSize,
       1,
       60,
       {
           B(LdaSmi8), U8(1),         //
           B(Star), R(2),             //
           B(Star), R(0),             //
           B(Star), R(3),             //
           B(LdaSmi8), U8(1),         //
           B(TestEqualStrict), R(3),  //
           B(JumpIfTrue), U8(10),     //
           B(LdaSmi8), U8(2),         //
           B(TestEqualStrict), R(3),  //
           B(JumpIfTrue), U8(36),     //
           B(Jump), U8(38),           //
           B(Ldar), R(2),             //
           B(Star), R(4),             //
           B(LdaSmi8), U8(1),         //
           B(Add), R(4),              //
           B(Star), R(1),             //
           B(Star), R(4),             //
           B(LdaSmi8), U8(2),         //
           B(TestEqualStrict), R(4),  //
           B(JumpIfTrue), U8(4),      //
           B(Jump), U8(8),            //
           B(LdaSmi8), U8(1),         //
           B(Star), R(2),             //
           B(Jump), U8(8),            //
           B(LdaSmi8), U8(2),         //
           B(Star), R(2),             //
           B(Jump), U8(2),            //
           B(LdaSmi8), U8(3),         //
           B(Star), R(2),             //
           B(LdaUndefined),           //
           B(Return),                 //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(BasicBlockToBoolean) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  // Check that we generate JumpIfToBoolean if they are at the start of basic
  // blocks.
  ExpectedSnippet<int> snippets[] = {
      {"var a = 1; if (a || a < 0) { return 1; }",
       2 * kPointerSize,
       1,
       20,
       {
           B(LdaSmi8), U8(1),               //
           B(Star), R(0),                   //
           B(JumpIfToBooleanTrue), U8(9),   //
           B(Ldar), R(0),                   //
           B(Star), R(1),                   //
           B(LdaZero),                      //
           B(TestLessThan), R(1),           //
           B(JumpIfToBooleanFalse), U8(5),  //
           B(LdaSmi8), U8(1),               //
           B(Return),                       //
           B(LdaUndefined),                 //
           B(Return),                       //
       }},
      {"var a = 1; if (a && a < 0) { return 1; }",
       2 * kPointerSize,
       1,
       20,
       {
           B(LdaSmi8), U8(1),               //
           B(Star), R(0),                   //
           B(JumpIfToBooleanFalse), U8(9),  //
           B(Ldar), R(0),                   //
           B(Star), R(1),                   //
           B(LdaZero),                      //
           B(TestLessThan), R(1),           //
           B(JumpIfToBooleanFalse), U8(5),  //
           B(LdaSmi8), U8(1),               //
           B(Return),                       //
           B(LdaUndefined),                 //
           B(Return),                       //
       }},
      {"var a = 1; a = (a || a < 0) ? 2 : 3;",
       2 * kPointerSize,
       1,
       25,
       {
           B(LdaSmi8), U8(1),               //
           B(Star), R(0),                   //
           B(JumpIfToBooleanTrue), U8(9),   //
           B(Ldar), R(0),                   //
           B(Star), R(1),                   //
           B(LdaZero),                      //
           B(TestLessThan), R(1),           //
           B(JumpIfToBooleanFalse), U8(6),  //
           B(LdaSmi8), U8(2),               //
           B(Jump), U8(4),                  //
           B(LdaSmi8), U8(3),               //
           B(Star), R(0),                   //
           B(LdaUndefined),                 //
           B(Return),                       //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(DeadCodeRemoval) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"return; var a = 1; a();",
       1 * kPointerSize,
       1,
       2,
       {
           B(LdaUndefined),  //
           B(Return),        //
       }},
      {"if (false) { return; }; var a = 1;",
       1 * kPointerSize,
       1,
       6,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(LdaUndefined),    //
           B(Return),          //
       }},
      {"if (true) { return 1; } else { return 2; };",
       0,
       1,
       3,
       {
           B(LdaSmi8), U8(1),  //
           B(Return),          //
       }},
      {"var a = 1; if (a) { return 1; }; return 2;",
       1 * kPointerSize,
       1,
       12,
       {
           B(LdaSmi8), U8(1),               //
           B(Star), R(0),                   //
           B(JumpIfToBooleanFalse), U8(5),  //
           B(LdaSmi8), U8(1),               //
           B(Return),                       //
           B(LdaSmi8), U8(2),               //
           B(Return),                       //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ThisFunction) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int closure = Register::function_closure().index();

  ExpectedSnippet<int> snippets[] = {
      {"var f;\n f = function f() { }",
       1 * kPointerSize,
       1,
       9,
       {
           B(LdaTheHole),        //
           B(Star), R(0),        //
           B(Ldar), R(closure),  //
           B(Star), R(0),        //
           B(LdaUndefined),      //
           B(Return),            //
       }},
      {"var f;\n f = function f() { return f; }",
       1 * kPointerSize,
       1,
       8,
       {
           B(LdaTheHole),        //
           B(Star), R(0),        //
           B(Ldar), R(closure),  //
           B(Star), R(0),        //
           B(Return),            //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunction(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(NewTarget) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int new_target = Register::new_target().index();

  ExpectedSnippet<int> snippets[] = {
      {"return new.target;",
       1 * kPointerSize,
       1,
       5,
       {
           B(Ldar), R(new_target),  //
           B(Star), R(0),           //
           B(Return),               //
       }},
      {"new.target;",
       1 * kPointerSize,
       1,
       6,
       {
           B(Ldar), R(new_target),  //
           B(Star), R(0),           //
           B(LdaUndefined),         //
           B(Return),               //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(RemoveRedundantLdar) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var ld_a = 1;\n"          // This test is to check Ldar does not
       "while(true) {\n"          // get removed if the preceding Star is
       "  ld_a = ld_a + ld_a;\n"  // in a different basicblock.
       "  if (ld_a > 10) break;\n"
       "}\n"
       "return ld_a;",
       2 * kPointerSize,
       1,
       29,
       {B(LdaSmi8), U8(1),         //
        B(Star), R(0),             //
        B(Ldar), R(0),             //  This load should not be removed as it
        B(Star), R(1),             //  is the target of the branch.
        B(Ldar), R(0),             //
        B(Add), R(1),              //
        B(Star), R(0),             //
        B(Star), R(1),             //
        B(LdaSmi8), U8(10),        //
        B(TestGreaterThan), R(1),  //
        B(JumpIfFalse), U8(4),     //
        B(Jump), U8(4),            //
        B(Jump), U8(-20),          //
        B(Ldar), R(0),             //
        B(Return)}},
      {"var ld_a = 1;\n"
       "do {\n"
       "  ld_a = ld_a + ld_a;\n"
       "  if (ld_a > 10) continue;\n"
       "} while(false);\n"
       "return ld_a;",
       2 * kPointerSize,
       1,
       27,
       {B(LdaSmi8), U8(1),         //
        B(Star), R(0),             //
        B(Ldar), R(0),             //
        B(Star), R(1),             //
        B(Ldar), R(0),             //
        B(Add), R(1),              //
        B(Star), R(0),             //
        B(Star), R(1),             //
        B(LdaSmi8), U8(10),        //
        B(TestGreaterThan), R(1),  //
        B(JumpIfFalse), U8(4),     //
        B(Jump), U8(2),            //
        B(Ldar), R(0),             //
        B(Return)}},
      {"var ld_a = 1;\n"
       "  ld_a = ld_a + ld_a;\n"
       "  return ld_a;",
       2 * kPointerSize,
       1,
       13,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(Star), R(1),      //
           B(Ldar), R(0),      //
           B(Add), R(1),       //
           B(Star), R(0),      //
           B(Return)           //
       }},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(AssignmentsInBinaryExpression) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<const char*> snippets[] = {
      {"var x = 0, y = 1;\n"
       "return (x = 2, y = 3, x = 4, y = 5)",
       2 * kPointerSize,
       1,
       24,
       {
           B(LdaZero), B(Star), R(0),  //
           B(LdaSmi8), U8(1),          //
           B(Star), R(1),              //
           B(LdaSmi8), U8(2),          //
           B(Star), R(0),              //
           B(LdaSmi8), U8(3),          //
           B(Star), R(1),              //
           B(LdaSmi8), U8(4),          //
           B(Star), R(0),              //
           B(LdaSmi8), U8(5),          //
           B(Star), R(1),              //
           B(Return),                  //
       },
       0},
      {"var x = 55;\n"
       "var y = (x = 100);\n"
       "return y",
       2 * kPointerSize,
       1,
       11,
       {
           B(LdaSmi8), U8(55),   //
           B(Star), R(0),        //
           B(LdaSmi8), U8(100),  //
           B(Star), R(0),        //
           B(Star), R(1),        //
           B(Return),            //
       },
       0},
      {"var x = 55;\n"
       "x = x + (x = 100) + (x = 101);\n"
       "return x;",
       3 * kPointerSize,
       1,
       23,
       {
           B(LdaSmi8), U8(55),   //
           B(Star), R(0),        //
           B(Star), R(1),        //
           B(LdaSmi8), U8(100),  //
           B(Star), R(0),        //
           B(Add), R(1),         //
           B(Star), R(2),        //
           B(LdaSmi8), U8(101),  //
           B(Star), R(0),        //
           B(Add), R(2),         //
           B(Star), R(0),        //
           B(Return),            //
       },
       0},
      {"var x = 55;\n"
       "x = (x = 56) - x + (x = 57);\n"
       "x++;\n"
       "return x;",
       3 * kPointerSize,
       1,
       31,
       {
           B(LdaSmi8), U8(55),  //
           B(Star), R(0),       //
           B(LdaSmi8), U8(56),  //
           B(Star), R(0),       //
           B(Star), R(1),       //
           B(Ldar), R(0),       //
           B(Sub), R(1),        //
           B(Star), R(2),       //
           B(LdaSmi8), U8(57),  //
           B(Star), R(0),       //
           B(Add), R(2),        //
           B(Star), R(0),       //
           B(ToNumber),         //
           B(Star), R(1),       //
           B(Inc),              //
           B(Star), R(0),       //
           B(Return),           //
       },
       0},
      {"var x = 55;\n"
       "var y = x + (x = 1) + (x = 2) + (x = 3);\n"
       "return y;",
       4 * kPointerSize,
       1,
       31,
       {
           B(LdaSmi8), U8(55),  //
           B(Star), R(0),       //
           B(Star), R(2),       //
           B(LdaSmi8), U8(1),   //
           B(Star), R(0),       //
           B(Add), R(2),        //
           B(Star), R(3),       //
           B(LdaSmi8), U8(2),   //
           B(Star), R(0),       //
           B(Add), R(3),        //
           B(Star), R(2),       //
           B(LdaSmi8), U8(3),   //
           B(Star), R(0),       //
           B(Add), R(2),        //
           B(Star), R(1),       //
           B(Return),           //
       },
       0},
      {"var x = 55;\n"
       "var x = x + (x = 1) + (x = 2) + (x = 3);\n"
       "return x;",
       3 * kPointerSize,
       1,
       31,
       {
           B(LdaSmi8), U8(55),  //
           B(Star), R(0),       //
           B(Star), R(1),       //
           B(LdaSmi8), U8(1),   //
           B(Star), R(0),       //
           B(Add), R(1),        //
           B(Star), R(2),       //
           B(LdaSmi8), U8(2),   //
           B(Star), R(0),       //
           B(Add), R(2),        //
           B(Star), R(1),       //
           B(LdaSmi8), U8(3),   //
           B(Star), R(0),       //
           B(Add), R(1),        //
           B(Star), R(0),       //
           B(Return),           //
       },
       0},
      {"var x = 10, y = 20;\n"
       "return x + (x = 1) + (x + 1) * (y = 2) + (y = 3) + (x = 4) + (y = 5) + "
       "y;\n",
       5 * kPointerSize,
       1,
       69,
       {
           B(LdaSmi8), U8(10),  //
           B(Star), R(0),       //
           B(LdaSmi8), U8(20),  //
           B(Star), R(1),       //
           B(Ldar), R(0),       //
           B(Star), R(2),       //
           B(LdaSmi8), U8(1),   //
           B(Star), R(0),       //
           B(Add), R(2),        //
           B(Star), R(3),       //
           B(Ldar), R(0),       //
           B(Star), R(2),       //
           B(LdaSmi8), U8(1),   //
           B(Add), R(2),        //
           B(Star), R(4),       //
           B(LdaSmi8), U8(2),   //
           B(Star), R(1),       //
           B(Mul), R(4),        //
           B(Add), R(3),        //
           B(Star), R(2),       //
           B(LdaSmi8), U8(3),   //
           B(Star), R(1),       //
           B(Add), R(2),        //
           B(Star), R(3),       //
           B(LdaSmi8), U8(4),   //
           B(Star), R(0),       //
           B(Add), R(3),        //
           B(Star), R(2),       //
           B(LdaSmi8), U8(5),   //
           B(Star), R(1),       //
           B(Add), R(2),        //
           B(Star), R(3),       //
           B(Ldar), R(1),       //
           B(Add), R(3),        //
           B(Return),           //
       },
       0},
      {"var x = 17;\n"
       "return 1 + x + (x++) + (++x);\n",
       4 * kPointerSize,
       1,
       37,
       {
           B(LdaSmi8), U8(17),  //
           B(Star), R(0),       //
           B(LdaSmi8), U8(1),   //
           B(Star), R(1),       //
           B(Ldar), R(0),       //
           B(Add), R(1),        //
           B(Star), R(2),       //
           B(Ldar), R(0),       //
           B(ToNumber),         //
           B(Star), R(1),       //
           B(Inc),              //
           B(Star), R(0),       //
           B(Ldar), R(1),       //
           B(Add), R(2),        //
           B(Star), R(3),       //
           B(Ldar), R(0),       //
           B(ToNumber),         //
           B(Inc),              //
           B(Star), R(0),       //
           B(Add), R(3),        //
           B(Return),           //
       },
       0}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(Eval) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int closure = Register::function_closure().index();
  int context = Register::function_context().index();
  int new_target = Register::new_target().index();

  int first_context_slot = Context::MIN_CONTEXT_SLOTS;

  ExpectedSnippet<const char*> snippets[] = {
      {"return eval('1;');",
       9 * kPointerSize,
       1,
       67,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),     //
                           U8(1),                                             //
           B(PushContext), R(0),                                              //
           B(Ldar), THIS(1),                                                  //
           B(StaContextSlot), R(0), U8(first_context_slot),                   //
           B(CreateMappedArguments),                                          //
           B(StaContextSlot), R(0), U8(first_context_slot + 1),               //
           B(Ldar), R(new_target),                                            //
           B(StaContextSlot), R(0), U8(first_context_slot + 2),               //
           B(Mov), R(context), R(3),                                          //
           B(LdaConstant), U8(0),                                             //
           B(Star), R(4),                                                     //
           B(CallRuntimeForPair), U16(Runtime::kLoadLookupSlot),              //
                                  R(3), U8(2), R(1),                          //
           B(LdaConstant), U8(1),                                             //
           B(Star), R(3),                                                     //
           B(Mov), R(1), R(4),                                                //
           B(Mov), R(3), R(5),                                                //
           B(Mov), R(closure), R(6),                                          //
           B(LdaZero),                                                        //
           B(Star), R(7),                                                     //
           B(LdaSmi8), U8(10),                                                //
           B(Star), R(8),                                                     //
           B(CallRuntime), U16(Runtime::kResolvePossiblyDirectEval), R(4),    //
                           U8(5),                                             //
           B(Star), R(1),                                                     //
           B(Call), R(1), R(2), U8(1), U8(0),                                 //
           B(Return),                                                         //
       },
       2,
       {"eval", "1;"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(LookupSlot) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int closure = Register::function_closure().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;
  int context = Register::function_context().index();
  int new_target = Register::new_target().index();

  ExpectedSnippet<const char*> snippets[] = {
      {"eval('var x = 10;'); return x;",
       9 * kPointerSize,
       1,
       69,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),     //
                           U8(1),                                             //
           B(PushContext), R(0),                                              //
           B(Ldar), THIS(1),                                                  //
           B(StaContextSlot), R(0), U8(first_context_slot),                   //
           B(CreateMappedArguments),                                          //
           B(StaContextSlot), R(0), U8(first_context_slot + 1),               //
           B(Ldar), R(new_target),                                            //
           B(StaContextSlot), R(0), U8(first_context_slot + 2),               //
           B(Mov), R(context), R(3),                                          //
           B(LdaConstant), U8(0),                                             //
           B(Star), R(4),                                                     //
           B(CallRuntimeForPair), U16(Runtime::kLoadLookupSlot),              //
                                  R(3), U8(2), R(1),                          //
           B(LdaConstant), U8(1),                                             //
           B(Star), R(3),                                                     //
           B(Mov), R(1), R(4),                                                //
           B(Mov), R(3), R(5),                                                //
           B(Mov), R(closure), R(6),                                          //
           B(LdaZero),                                                        //
           B(Star), R(7),                                                     //
           B(LdaSmi8), U8(10),                                                //
           B(Star), R(8),                                                     //
           B(CallRuntime), U16(Runtime::kResolvePossiblyDirectEval), R(4),    //
                           U8(5),                                             //
           B(Star), R(1),                                                     //
           B(Call), R(1), R(2), U8(1), U8(0),                                 //
           B(LdaLookupSlot), U8(2),                                           //
           B(Return),                                                         //
       },
       3,
       {"eval", "var x = 10;", "x"}},
      {"eval('var x = 10;'); return typeof x;",
        9 * kPointerSize,
        1,
        70,
        {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),     //
                           U8(1),                                             //
           B(PushContext), R(0),                                              //
           B(Ldar), THIS(1),                                                  //
           B(StaContextSlot), R(0), U8(first_context_slot),                   //
           B(CreateMappedArguments),                                          //
           B(StaContextSlot), R(0), U8(first_context_slot + 1),               //
           B(Ldar), R(new_target),                                            //
           B(StaContextSlot), R(0), U8(first_context_slot + 2),               //
           B(Mov), R(context), R(3),                                          //
           B(LdaConstant), U8(0),                                             //
           B(Star), R(4),                                                     //
           B(CallRuntimeForPair), U16(Runtime::kLoadLookupSlot),              //
                                  R(3), U8(2), R(1),                          //
           B(LdaConstant), U8(1),                                             //
           B(Star), R(3),                                                     //
           B(Mov), R(1), R(4),                                                //
           B(Mov), R(3), R(5),                                                //
           B(Mov), R(closure), R(6),                                          //
           B(LdaZero),                                                        //
           B(Star), R(7),                                                     //
           B(LdaSmi8), U8(10),                                                //
           B(Star), R(8),                                                     //
           B(CallRuntime), U16(Runtime::kResolvePossiblyDirectEval), R(4),    //
                           U8(5),                                             //
           B(Star), R(1),                                                     //
           B(Call), R(1), R(2), U8(1), U8(0),                                 //
           B(LdaLookupSlotInsideTypeof), U8(2),                               //
           B(TypeOf),                                                         //
           B(Return),                                                         //
       },
       3,
       {"eval", "var x = 10;", "x"}},
      {"x = 20; return eval('');",
       9 * kPointerSize,
       1,
       71,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),     //
                           U8(1),                                             //
           B(PushContext), R(0),                                              //
           B(Ldar), THIS(1),                                                  //
           B(StaContextSlot), R(0), U8(first_context_slot),                   //
           B(CreateMappedArguments),                                          //
           B(StaContextSlot), R(0), U8(first_context_slot + 1),               //
           B(Ldar), R(new_target),                                            //
           B(StaContextSlot), R(0), U8(first_context_slot + 2),               //
           B(LdaSmi8), U8(20),                                                //
           B(StaLookupSlotSloppy), U8(0),                                     //
           B(Mov), R(context), R(3),                                          //
           B(LdaConstant), U8(1),                                             //
           B(Star), R(4),                                                     //
           B(CallRuntimeForPair), U16(Runtime::kLoadLookupSlot),              //
                                  R(3), U8(2), R(1),                          //
           B(LdaConstant), U8(2),                                             //
           B(Star), R(3),                                                     //
           B(Mov), R(1), R(4),                                                //
           B(Mov), R(3), R(5),                                                //
           B(Mov), R(closure), R(6),                                          //
           B(LdaZero),                                                        //
           B(Star), R(7),                                                     //
           B(LdaSmi8), U8(10),                                                //
           B(Star), R(8),                                                     //
           B(CallRuntime), U16(Runtime::kResolvePossiblyDirectEval), R(4),    //
                           U8(5),                                             //
           B(Star), R(1),                                                     //
           B(Call), R(1), R(2), U8(1), U8(0),                                 //
           B(Return),                                                         //
       },
       3,
       {"x", "eval", ""}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(CallLookupSlot) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddLoadICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddCallICSlot();
  USE(slot1);

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  int closure = Register::function_closure().index();
  int context = Register::function_context().index();
  int new_target = Register::new_target().index();

  ExpectedSnippet<InstanceType> snippets[] = {
      {"g = function(){}; eval(''); return g();",
       9 * kPointerSize,
       1,
       90,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext), R(closure),   //
                           U8(1),                                           //
           B(PushContext), R(0),                                            //
           B(Ldar), THIS(1),                                                //
           B(StaContextSlot), R(0), U8(4),                                  //
           B(CreateMappedArguments),                                        //
           B(StaContextSlot), R(0), U8(5),                                  //
           B(Ldar), R(new_target),                                          //
           B(StaContextSlot), R(0), U8(6),                                  //
           B(CreateClosure), U8(0), U8(0),                                  //
           B(StaLookupSlotSloppy), U8(1),                                   //
           B(Mov), R(context), R(3),                                        //
           B(LdaConstant), U8(2),                                           //
           B(Star), R(4),                                                   //
           B(CallRuntimeForPair), U16(Runtime::kLoadLookupSlot),            //
                                  R(3), U8(2), R(1),                        //
           B(LdaConstant), U8(3),                                           //
           B(Star), R(3),                                                   //
           B(Mov), R(1), R(4),                                              //
           B(Mov), R(3), R(5),                                              //
           B(Mov), R(closure), R(6),                                        //
           B(LdaZero),                                                      //
           B(Star), R(7),                                                   //
           B(LdaSmi8), U8(10),                                              //
           B(Star), R(8),                                                   //
           B(CallRuntime), U16(Runtime::kResolvePossiblyDirectEval), R(4),  //
                           U8(5),                                           //
           B(Star), R(1),                                                   //
           B(Call), R(1), R(2), U8(1), U8(0),                               //
           B(Mov), R(context), R(3),                                        //
           B(LdaConstant), U8(1),                                           //
           B(Star), R(4),                                                   //
           B(CallRuntimeForPair), U16(Runtime::kLoadLookupSlot),            //
                                  R(3), U8(2), R(1),                        //
           B(Call), R(1), R(2), U8(0), U8(vector->GetIndex(slot2)),         //
           B(Return),                                                       //
       },
       4,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(LookupSlotInEval) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  const char* function_prologue = "var f;"
                                  "var x = 1;"
                                  "function f1() {"
                                  "  eval(\"function t() {";
  const char* function_epilogue = "        }; f = t; f();\");"
                                  "}"
                                  "f1();";

  ExpectedSnippet<const char*> snippets[] = {
      {"return x;",
       0 * kPointerSize,
       1,
       3,
       {
           B(LdaLookupSlot), U8(0),  //
           B(Return)                 //
       },
       1,
       {"x"}},
      {"x = 10;",
       0 * kPointerSize,
       1,
       6,
       {
           B(LdaSmi8), U8(10),             //
           B(StaLookupSlotSloppy), U8(0),  //
           B(LdaUndefined),                //
           B(Return),                      //
       },
       1,
       {"x"}},
      {"'use strict'; x = 10;",
       0 * kPointerSize,
       1,
       6,
       {
           B(LdaSmi8), U8(10),             //
           B(StaLookupSlotStrict), U8(0),  //
           B(LdaUndefined),                //
           B(Return),                      //
       },
       1,
       {"x"}},
      {"return typeof x;",
       0 * kPointerSize,
       1,
       4,
       {
           B(LdaLookupSlotInsideTypeof), U8(0),  //
           B(TypeOf),                            //
           B(Return),                            //
       },
       1,
       {"x"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    std::string script = std::string(function_prologue) +
                         std::string(snippets[i].code_snippet) +
                         std::string(function_epilogue);
    // TODO(mythria): use * as filter when function declarations are supported
    // inside eval.
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(script.c_str(), "t", "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(LookupSlotWideInEval) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  const char* function_prologue =
      "var f;"
      "var x = 1;"
      "function f1() {"
      "  eval(\"function t() {";
  const char* function_epilogue =
      "        }; f = t; f();\");"
      "}"
      "f1();";

  int const_count[] = {0, 0, 0, 0};
  ExpectedSnippet<InstanceType, 257> snippets[] = {
      {REPEAT_256(SPACE, "var y = 2.3;")
       "return x;",
       1 * kPointerSize,
       1,
       1028,
       {
           REPEAT_256(SPACE,                         //
             B(LdaConstant), U8(const_count[0]++),   //
             B(Star), R(0), )                        //
           B(LdaLookupSlotWide), U16(256),           //
           B(Return)                                 //
       },
       257,
       {REPEAT_256(COMMA, InstanceType::HEAP_NUMBER_TYPE),
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {REPEAT_256(SPACE, "var y = 2.3;")
       "return typeof x;",
       1 * kPointerSize,
       1,
       1029,
       {
           REPEAT_256(SPACE,                            //
             B(LdaConstant), U8(const_count[1]++),      //
             B(Star), R(0), )                           //
           B(LdaLookupSlotInsideTypeofWide), U16(256),  //
           B(TypeOf),                                   //
           B(Return)                                    //
       },
       257,
       {REPEAT_256(COMMA, InstanceType::HEAP_NUMBER_TYPE),
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {REPEAT_256(SPACE, "var y = 2.3;")
       "x = 10;",
       1 * kPointerSize,
       1,
       1031,
       {
           REPEAT_256(SPACE,                        //
             B(LdaConstant), U8(const_count[2]++),  //
             B(Star), R(0), )                       //
           B(LdaSmi8), U8(10),                      //
           B(StaLookupSlotSloppyWide), U16(256),    //
           B(LdaUndefined),                         //
           B(Return)                                //
       },
       257,
       {REPEAT_256(COMMA, InstanceType::HEAP_NUMBER_TYPE),
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"'use strict';"
       REPEAT_256(SPACE, "var y = 2.3;")
       "x = 10;",
       1 * kPointerSize,
       1,
       1031,
       {
           REPEAT_256(SPACE,
             B(LdaConstant), U8(const_count[3]++),  //
             B(Star), R(0), )                       //
           B(LdaSmi8), U8(10),                      //
           B(StaLookupSlotStrictWide), U16(256),    //
           B(LdaUndefined),                         //
           B(Return)                                //
       },
       257,
       {REPEAT_256(COMMA, InstanceType::HEAP_NUMBER_TYPE),
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    std::string script = std::string(function_prologue) +
                         std::string(snippets[i].code_snippet) +
                         std::string(function_epilogue);
    // TODO(mythria): use * as filter when function declarations are supported
    // inside eval.
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(script.c_str(), "t", "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(DeleteLookupSlotInEval) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  const char* function_prologue = "var f;"
                                  "var x = 1;"
                                  "z = 10;"
                                  "function f1() {"
                                  "  var y;"
                                  "  eval(\"function t() {";
  const char* function_epilogue = "        }; f = t; f();\");"
                                  "}"
                                  "f1();";

  ExpectedSnippet<const char*> snippets[] = {
      {"delete x;",
       0 * kPointerSize,
       1,
       5,
       {
           B(LdaConstant), U8(0),  //
           B(DeleteLookupSlot),    //
           B(LdaUndefined),        //
           B(Return)               //
       },
       1,
       {"x"}},
      {"return delete y;",
       0 * kPointerSize,
       1,
       2,
       {
           B(LdaFalse),        //
           B(Return)           //
       },
       0},
      {"return delete z;",
       0 * kPointerSize,
       1,
       4,
       {
           B(LdaConstant), U8(0),  //
           B(DeleteLookupSlot),    //
           B(Return)               //
       },
       1,
       {"z"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    std::string script = std::string(function_prologue) +
                         std::string(snippets[i].code_snippet) +
                         std::string(function_epilogue);
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(script.c_str(), "t", "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
