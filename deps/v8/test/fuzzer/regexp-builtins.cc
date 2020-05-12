// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>

#include "include/v8.h"
#include "src/heap/factory.h"
#include "src/objects/objects-inl.h"
#include "src/regexp/regexp.h"
#include "test/fuzzer/fuzzer-support.h"

// This is a hexdump of test/fuzzer/regexp_builtins/mjsunit.js generated using
// `xxd -i mjsunit.js`. It contains the `assertEquals` JS function used below.
#include "test/fuzzer/regexp_builtins/mjsunit.js.h"

namespace v8 {
namespace internal {
namespace {

constexpr bool kVerbose = false;  // For debugging, verbose error messages.
constexpr uint32_t kRegExpBuiltinsFuzzerHashSeed = 83;

#define REGEXP_BUILTINS(V)   \
  V(Exec, exec)              \
  V(Match, Symbol.match)     \
  V(Replace, Symbol.replace) \
  V(Search, Symbol.search)   \
  V(Split, Symbol.split)     \
  V(Test, test)

struct FuzzerArgs {
  FuzzerArgs(const uint8_t* input_data, size_t input_length,
             v8::Local<v8::Context> context, Isolate* isolate)
      : input_cursor(0),
        input_data(input_data),
        input_length(input_length),
        context(context),
        isolate(isolate) {}

  size_t input_cursor;
  const uint8_t* const input_data;
  const size_t input_length;
  v8::Local<v8::Context> context;
  Isolate* const isolate;
};

enum RegExpBuiltin {
#define CASE(name, ...) kRegExpPrototype##name,
  REGEXP_BUILTINS(CASE)
#undef CASE
      kRegExpBuiltinCount,
};

#define CASE(name, ...) void TestRegExpPrototype##name(FuzzerArgs* args);
REGEXP_BUILTINS(CASE)
#undef CASE

v8::Local<v8::String> v8_str(v8::Isolate* isolate, const char* s) {
  return v8::String::NewFromUtf8(isolate, s).ToLocalChecked();
}

v8::MaybeLocal<v8::Value> CompileRun(v8::Local<v8::Context> context,
                                     const char* source) {
  v8::Local<v8::Script> script;
  v8::MaybeLocal<v8::Script> maybe_script =
      v8::Script::Compile(context, v8_str(context->GetIsolate(), source));

  if (!maybe_script.ToLocal(&script)) return v8::MaybeLocal<v8::Value>();
  return script->Run(context);
}

uint8_t RandomByte(FuzzerArgs* args) {
  // Silently wraps to the beginning of input data. Ideally, input data should
  // be long enough to avoid that.
  const size_t index = args->input_cursor;
  CHECK(index < args->input_length);
  args->input_cursor = (index + 1) % args->input_length;
  return args->input_data[index];
}

void CompileMjsunit(const FuzzerArgs* args) {
  std::string source(
      reinterpret_cast<const char*>(test_fuzzer_regexp_builtins_mjsunit_js),
      test_fuzzer_regexp_builtins_mjsunit_js_len);
  CompileRun(args->context, source.c_str()).ToLocalChecked();
}

std::string NaiveEscape(const std::string& input, char escaped_char) {
  std::string out;
  for (size_t i = 0; i < input.size(); i++) {
    // Just omit newlines and \0 chars and naively replace other escaped chars.
    const char c = input[i];
    if (c == '\r' || c == '\n' || c == '\0') continue;
    out += (input[i] == escaped_char) ? '_' : c;
  }
  // Disallow trailing backslashes as they mess with our naive source string
  // concatenation.
  if (!out.empty() && out.back() == '\\') out.back() = '_';

  return out;
}

std::string GenerateRandomString(FuzzerArgs* args, size_t length) {
  // Limited to an ASCII subset for now.
  std::string s(length, '\0');
  for (size_t i = 0; i < length; i++) {
    s[i] = static_cast<char>((RandomByte(args) % 0x5F) + 0x20);
  }

  return s;
}

std::string GenerateRandomPattern(FuzzerArgs* args) {
  const int kMaxPatternLength = 16;
  std::string s =
      GenerateRandomString(args, (RandomByte(args) % kMaxPatternLength) + 1);
  // A leading '*' would be a comment instead of a regexp literal.
  if (s[0] == '*') s[0] = '.';
  return s;
}

std::string PickRandomPresetPattern(FuzzerArgs* args) {
  static const char* preset_patterns[] = {
      ".",         // Always matches.
      "\\P{Any}",  // Never matches.
      "^",         // Zero-width assertion, matches once.
      "(?=.)",     // Zero-width assertion, matches at every position.
      "\\b",       // Zero-width assertion, matches at each word boundary.
      "()",      // Zero-width assertion, matches at every position with groups.
      "(?<a>)",  // Likewise but with named groups.
      "((((.).).).)", "(?<a>(?<b>(?<c>(?<d>.).).).)",
      // Copied from
      // https://cs.chromium.org/chromium/src/testing/libfuzzer/fuzzers/dicts/regexp.dict
      "?", "abc", "()", "[]", "abc|def", "abc|def|ghi", "^xxx$",
      "ab\\b\\d\\bcd", "\\w|\\d", "a*?", "abc+", "abc+?", "xyz?", "xyz??",
      "xyz{0,1}", "xyz{0,1}?", "xyz{93}", "xyz{1,32}", "xyz{1,32}?", "xyz{1,}",
      "xyz{1,}?", "a\\fb\\nc\\rd\\te\\vf", "a\\nb\\bc", "(?:foo)", "(?: foo )",
      "foo|(bar|baz)|quux", "foo(?=bar)baz", "foo(?!bar)baz", "foo(?<=bar)baz",
      "foo(?<!bar)baz", "()", "(?=)", "[]", "[x]", "[xyz]", "[a-zA-Z0-9]",
      "[-123]", "[^123]", "]", "}", "[a-b-c]", "[x\\dz]", "[\\d-z]",
      "[\\d-\\d]", "[z-\\d]", "\\cj\\cJ\\ci\\cI\\ck\\cK", "\\c!", "\\c_",
      "\\c~", "[\\c!]", "[\\c_]", "[\\c~]", "[\\ca]", "[\\cz]", "[\\cA]",
      "[\\cZ]", "[\\c1]", "\\[\\]\\{\\}\\(\\)\\%\\^\\#\\ ",
      "[\\[\\]\\{\\}\\(\\)\\%\\^\\#\\ ]", "\\8", "\\9", "\\11", "\\11a",
      "\\011", "\\118", "\\111", "\\1111", "(x)(x)(x)\\1", "(x)(x)(x)\\2",
      "(x)(x)(x)\\3", "(x)(x)(x)\\4", "(x)(x)(x)\\1*", "(x)(x)(x)\\3*",
      "(x)(x)(x)\\4*", "(x)(x)(x)(x)(x)(x)(x)(x)(x)(x)\\10",
      "(x)(x)(x)(x)(x)(x)(x)(x)(x)(x)\\11", "(a)\\1", "(a\\1)", "(\\1a)",
      "(\\2)(\\1)", "(?=a){0,10}a", "(?=a){1,10}a", "(?=a){9,10}a", "(?!a)?a",
      "\\1(a)", "(?!(a))\\1", "(?!\\1(a\\1)\\1)\\1",
      "\\1\\2(a(?:\\1(b\\1\\2))\\2)\\1", "[\\0]", "[\\11]", "[\\11a]",
      "[\\011]", "[\\00011]", "[\\118]", "[\\111]", "[\\1111]", "\\x60",
      "\\x3z", "\\c", "\\u0034", "\\u003z", "foo[z]*", "\\u{12345}",
      "\\u{12345}\\u{23456}", "\\u{12345}{3}", "\\u{12345}*", "\\ud808\\udf45*",
      "[\\ud808\\udf45-\\ud809\\udccc]", "a", "a|b", "a\\n", "a$", "a\\b!",
      "a\\Bb", "a*?", "a?", "a??", "a{0,1}?", "a{1,2}?", "a+?", "(a)", "(a)\\1",
      "(\\1a)", "\\1(a)", "a\\s", "a\\S", "a\\D", "a\\w", "a\\W", "a.", "a\\q",
      "a[a]", "a[^a]", "a[a-z]", "a(?:b)", "a(?=b)", "a(?!b)", "\\x60",
      "\\u0060", "\\cA", "\\q", "\\1112", "(a)\\1", "(?!a)?a\\1",
      "(?:(?=a))a\\1", "a{}", "a{,}", "a{", "a{z}", "a{12z}", "a{12,",
      "a{12,3b", "{}", "{,}", "{", "{z}", "{1z}", "{12,", "{12,3b", "a", "abc",
      "a[bc]d", "a|bc", "ab|c", "a||bc", "(?:ab)", "(?:ab|cde)", "(?:ab)|cde",
      "(ab)", "(ab|cde)", "(ab)\\1", "(ab|cde)\\1", "(?:ab)?", "(?:ab)+", "a?",
      "a+", "a??", "a*?", "a+?", "(?:a?)?", "(?:a+)?", "(?:a?)+", "(?:a*)+",
      "(?:a+)+", "(?:a?)*", "(?:a*)*", "(?:a+)*", "a{0}", "(?:a+){0,0}", "a*b",
      "a+b", "a*b|c", "a+b|c", "(?:a{5,1000000}){3,1000000}", "(?:ab){4,7}",
      "a\\bc", "a\\sc", "a\\Sc", "a(?=b)c", "a(?=bbb|bb)c", "a(?!bbb|bb)c",
      "\xe2\x81\xa3", "[\xe2\x81\xa3]", "\xed\xb0\x80", "\xed\xa0\x80",
      "(\xed\xb0\x80)\x01", "((\xed\xa0\x80))\x02", "\xf0\x9f\x92\xa9", "\x01",
      "\x0f", "[-\xf0\x9f\x92\xa9]+", "[\xf0\x9f\x92\xa9-\xf4\x8f\xbf\xbf]",
      "(?<=)", "(?<=a)", "(?<!)", "(?<!a)", "(?<a>)", "(?<a>.)",
      "(?<a>.)\\k<a>", "\\p{Script=Greek}", "\\P{sc=Greek}",
      "\\p{Script_Extensions=Greek}", "\\P{scx=Greek}",
      "\\p{General_Category=Decimal_Number}", "\\P{gc=Decimal_Number}",
      "\\p{gc=Nd}", "\\P{Decimal_Number}", "\\p{Nd}", "\\P{Any}",
      "\\p{Changes_When_NFKC_Casefolded}",
  };
  static constexpr int preset_pattern_count = arraysize(preset_patterns);
  STATIC_ASSERT(preset_pattern_count < 0xFF);

  return std::string(preset_patterns[RandomByte(args) % preset_pattern_count]);
}

std::string PickPattern(FuzzerArgs* args) {
  if ((RandomByte(args) & 3) == 0) {
    return NaiveEscape(GenerateRandomPattern(args), '/');
  } else {
    return PickRandomPresetPattern(args);
  }
}

std::string GenerateRandomString(FuzzerArgs* args) {
  const int kMaxStringLength = 64;
  return GenerateRandomString(args, RandomByte(args) % kMaxStringLength);
}

std::string PickSubjectString(FuzzerArgs* args) {
  if ((RandomByte(args) & 0xF) == 0) {
    // Sometimes we have a two-byte subject string.
    return "f\\uD83D\\uDCA9ba\\u2603";
  } else {
    return NaiveEscape(GenerateRandomString(args), '\'');
  }
}

std::string PickReplacementForReplace(FuzzerArgs* args) {
  static const char* candidates[] = {
      "'X'",
      "'$1$2$3'",
      "'$$$&$`$\\'$1'",
      "() => 'X'",
      "(arg0, arg1, arg2, arg3, arg4) => arg0 + arg1 + arg2 + arg3 + arg4",
      "() => 42",
  };
  static const int candidate_count = arraysize(candidates);

  if ((RandomByte(args) & 1) == 0) {
    return candidates[RandomByte(args) % candidate_count];
  } else {
    return std::string("'") + NaiveEscape(GenerateRandomString(args), '\'') +
           std::string("'");
  }
}

std::string PickLimitForSplit(FuzzerArgs* args) {
  // clang-format off
  switch (RandomByte(args) & 0x3) {
  case 0: return "undefined";
  case 1: return "'not a number'";
  case 2: return std::to_string(Smi::kMaxValue + RandomByte(args));
  case 3: return std::to_string(RandomByte(args));
  default: UNREACHABLE();
  }  // clang-format on
}

std::string GenerateRandomFlags(FuzzerArgs* args) {
  constexpr size_t kFlagCount = JSRegExp::kFlagCount;
  CHECK_EQ(JSRegExp::kDotAll, 1 << (kFlagCount - 1));
  STATIC_ASSERT((1 << kFlagCount) - 1 < 0xFF);

  const size_t flags = RandomByte(args) & ((1 << kFlagCount) - 1);

  int cursor = 0;
  char buffer[kFlagCount] = {'\0'};

  if (flags & JSRegExp::kGlobal) buffer[cursor++] = 'g';
  if (flags & JSRegExp::kIgnoreCase) buffer[cursor++] = 'i';
  if (flags & JSRegExp::kMultiline) buffer[cursor++] = 'm';
  if (flags & JSRegExp::kSticky) buffer[cursor++] = 'y';
  if (flags & JSRegExp::kUnicode) buffer[cursor++] = 'u';
  if (flags & JSRegExp::kDotAll) buffer[cursor++] = 's';

  return std::string(buffer, cursor);
}

std::string GenerateRandomLastIndex(FuzzerArgs* args) {
  static const char* candidates[] = {
      "undefined",  "-1",         "0",
      "1",          "2",          "3",
      "4",          "5",          "6",
      "7",          "8",          "9",
      "50",         "4294967296", "2147483647",
      "2147483648", "NaN",        "Not a Number",
  };
  static const int candidate_count = arraysize(candidates);
  return candidates[RandomByte(args) % candidate_count];
}

void RunTest(FuzzerArgs* args) {
  switch (RandomByte(args) % kRegExpBuiltinCount) {
#define CASE(name, ...)              \
  case kRegExpPrototype##name:       \
    TestRegExpPrototype##name(args); \
    break;
    REGEXP_BUILTINS(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

std::string GenerateSourceString(FuzzerArgs* args, const std::string& test) {
  std::string pattern = PickPattern(args);
  std::string flags = GenerateRandomFlags(args);
  std::string last_index = GenerateRandomLastIndex(args);
  std::string subject = PickSubjectString(args);

  // clang-format off
  std::stringstream ss;
  ss << "function test() {\n"
     << "  const re = /" << pattern<< "/"
                         << flags << ";\n"
     << "  re.lastIndex = " << last_index << ";\n"
     << "  const str = '" << subject << "';\n"
     << "  let result = null;\n"
     << "  let exception = null;\n"
     << "  try {\n"
     << "    result = " << test << "\n"
     << "  } catch (e) {\n"
     << "    exception = e;\n"
     << "  }\n"
     << "  return { result: result, re: re, exception: exception };\n"
     << "}\n"
     << "%SetForceSlowPath(false);\n"
     << "test();  // Run once ahead of time to compile the regexp.\n"
     << "const fast = test();\n"
     << "%SetForceSlowPath(true);\n"
     << "const slow = test();\n"
     << "%SetForceSlowPath(false);\n";
  // clang-format on

  std::string source = ss.str();
  if (kVerbose) {
    fprintf(stderr, "Generated source:\n```\n%s\n```\n", source.c_str());
  }

  return source;
}

void PrintExceptionMessage(v8::Isolate* isolate, v8::TryCatch* try_catch) {
  CHECK(try_catch->HasCaught());
  static const int kBufferLength = 256;
  char buffer[kBufferLength + 1];
  try_catch->Message()->Get()->WriteOneByte(
      isolate, reinterpret_cast<uint8_t*>(&buffer[0]), 0, kBufferLength);
  fprintf(stderr, "%s\n", buffer);
}

bool ResultsAreIdentical(FuzzerArgs* args) {
  std::string source =
      "assertEquals(fast.exception, slow.exception);\n"
      "assertEquals(fast.result, slow.result);\n"
      "if (fast.result !== null) {\n"
      "  assertEquals(fast.result.groups, slow.result.groups);\n"
      "  assertEquals(fast.result.indices, slow.result.indices);\n"
      "  if (fast.result.indices !== undefined) {\n"
      "    assertEquals(fast.result.indices.groups,\n"
      "                 slow.result.indices.groups);\n"
      "  }\n"
      "}\n"
      "assertEquals(fast.re.lastIndex, slow.re.lastIndex);\n";

  v8::Local<v8::Value> result;
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(args->isolate);
  v8::TryCatch try_catch(isolate);
  if (!CompileRun(args->context, source.c_str()).ToLocal(&result)) {
    PrintExceptionMessage(isolate, &try_catch);
    args->isolate->clear_pending_exception();
    return false;
  }

  return true;
}

void CompileRunAndVerify(FuzzerArgs* args, const std::string& source) {
  v8::Local<v8::Value> result;
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(args->isolate);
  v8::TryCatch try_catch(isolate);
  if (!CompileRun(args->context, source.c_str()).ToLocal(&result)) {
    args->isolate->clear_pending_exception();
    // No need to verify result if an exception was thrown here, since that
    // implies a syntax error somewhere in the pattern or string. We simply
    // ignore those.
    if (kVerbose) {
      PrintExceptionMessage(isolate, &try_catch);
      fprintf(stderr, "Failed to run script:\n```\n%s\n```\n", source.c_str());
    }
    return;
  }

  if (!ResultsAreIdentical(args)) {
    uint32_t hash = StringHasher::HashSequentialString(
        args->input_data, static_cast<int>(args->input_length),
        kRegExpBuiltinsFuzzerHashSeed);
    FATAL("!ResultAreIdentical(args); RegExpBuiltinsFuzzerHash=%x", hash);
  }
}

void TestRegExpPrototypeExec(FuzzerArgs* args) {
  std::string test = "re.exec(str);";
  std::string source = GenerateSourceString(args, test);
  CompileRunAndVerify(args, source);
}

void TestRegExpPrototypeMatch(FuzzerArgs* args) {
  std::string test = "re[Symbol.match](str);";
  std::string source = GenerateSourceString(args, test);
  CompileRunAndVerify(args, source);
}

void TestRegExpPrototypeReplace(FuzzerArgs* args) {
  std::string replacement = PickReplacementForReplace(args);
  std::string test = "re[Symbol.replace](str, " + replacement + ");";
  std::string source = GenerateSourceString(args, test);
  CompileRunAndVerify(args, source);
}

void TestRegExpPrototypeSearch(FuzzerArgs* args) {
  std::string test = "re[Symbol.search](str);";
  std::string source = GenerateSourceString(args, test);
  CompileRunAndVerify(args, source);
}

void TestRegExpPrototypeSplit(FuzzerArgs* args) {
  std::string limit = PickLimitForSplit(args);
  std::string test = "re[Symbol.split](str, " + limit + ");";
  std::string source = GenerateSourceString(args, test);
  CompileRunAndVerify(args, source);
}

void TestRegExpPrototypeTest(FuzzerArgs* args) {
  std::string test = "re.test(str);";
  std::string source = GenerateSourceString(args, test);
  CompileRunAndVerify(args, source);
}

#undef REGEXP_BUILTINS

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 64) return 0;  // Need a minimal amount of randomness to do stuff.

  // Flag definitions.

  FLAG_allow_natives_syntax = true;

  // V8 setup.

  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = support->GetContext();
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);

  CHECK(!i_isolate->has_pending_exception());

  // And run.

  FuzzerArgs args(data, size, context, i_isolate);
  CompileMjsunit(&args);
  RunTest(&args);

  CHECK(!i_isolate->has_pending_exception());
  return 0;
}

}  // namespace internal
}  // namespace v8
