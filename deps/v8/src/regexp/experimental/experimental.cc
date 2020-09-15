// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/experimental/experimental.h"

#include "src/objects/js-regexp-inl.h"
#include "src/regexp/experimental/experimental-compiler.h"
#include "src/regexp/experimental/experimental-interpreter.h"
#include "src/regexp/regexp-parser.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

bool ExperimentalRegExp::CanBeHandled(RegExpTree* tree, JSRegExp::Flags flags,
                                      int capture_count, Zone* zone) {
  return ExperimentalRegExpCompiler::CanBeHandled(tree, flags, capture_count,
                                                  zone);
}

void ExperimentalRegExp::Initialize(Isolate* isolate, Handle<JSRegExp> re,
                                    Handle<String> source,
                                    JSRegExp::Flags flags, int capture_count) {
  DCHECK(FLAG_enable_experimental_regexp_engine);
  if (FLAG_trace_experimental_regexp_engine) {
    StdoutStream{} << "Initializing experimental regexp " << *source
                   << std::endl;
  }

  isolate->factory()->SetRegExpExperimentalData(re, source, flags,
                                                capture_count);
}

bool ExperimentalRegExp::IsCompiled(Handle<JSRegExp> re, Isolate* isolate) {
  DCHECK(FLAG_enable_experimental_regexp_engine);

  DCHECK_EQ(re->TypeTag(), JSRegExp::EXPERIMENTAL);
#ifdef VERIFY_HEAP
  re->JSRegExpVerify(isolate);
#endif

  return re->DataAt(JSRegExp::kIrregexpLatin1BytecodeIndex) !=
         Smi::FromInt(JSRegExp::kUninitializedValue);
}

void ExperimentalRegExp::Compile(Isolate* isolate, Handle<JSRegExp> re) {
  DCHECK_EQ(re->TypeTag(), JSRegExp::EXPERIMENTAL);
#ifdef VERIFY_HEAP
  re->JSRegExpVerify(isolate);
#endif

  Handle<String> source(re->Pattern(), isolate);
  if (FLAG_trace_experimental_regexp_engine) {
    StdoutStream{} << "Compiling experimental regexp " << *source << std::endl;
  }

  Zone zone(isolate->allocator(), ZONE_NAME);

  // Parse and compile the regexp source.
  RegExpCompileData parse_result;
  JSRegExp::Flags flags = re->GetFlags();
  FlatStringReader reader(isolate, source);
  DCHECK(!isolate->has_pending_exception());

  // The pattern was already parsed during initialization, so it should never
  // fail here:
  bool parse_success =
      RegExpParser::ParseRegExp(isolate, &zone, &reader, flags, &parse_result);
  CHECK(parse_success);

  ZoneList<RegExpInstruction> bytecode =
      ExperimentalRegExpCompiler::Compile(parse_result.tree, flags, &zone);

  int byte_length = sizeof(RegExpInstruction) * bytecode.length();
  Handle<ByteArray> bytecode_byte_array =
      isolate->factory()->NewByteArray(byte_length);
  MemCopy(bytecode_byte_array->GetDataStartAddress(), bytecode.begin(),
          byte_length);

  re->SetDataAt(JSRegExp::kIrregexpLatin1BytecodeIndex, *bytecode_byte_array);
  re->SetDataAt(JSRegExp::kIrregexpUC16BytecodeIndex, *bytecode_byte_array);

  Handle<Code> trampoline = BUILTIN_CODE(isolate, RegExpExperimentalTrampoline);
  re->SetDataAt(JSRegExp::kIrregexpLatin1CodeIndex, *trampoline);
  re->SetDataAt(JSRegExp::kIrregexpUC16CodeIndex, *trampoline);
}

Vector<RegExpInstruction> AsInstructionSequence(ByteArray raw_bytes) {
  RegExpInstruction* inst_begin =
      reinterpret_cast<RegExpInstruction*>(raw_bytes.GetDataStartAddress());
  int inst_num = raw_bytes.length() / sizeof(RegExpInstruction);
  DCHECK_EQ(sizeof(RegExpInstruction) * inst_num, raw_bytes.length());
  return Vector<RegExpInstruction>(inst_begin, inst_num);
}

using MatchRange = ExperimentalRegExpInterpreter::MatchRange;

// Returns the number of matches.
int32_t ExperimentalRegExp::ExecRaw(JSRegExp regexp, String subject,
                                    int32_t* output_registers,
                                    int32_t output_register_count,
                                    int32_t subject_index) {
  DisallowHeapAllocation no_gc;

  DCHECK(FLAG_enable_experimental_regexp_engine);

  if (FLAG_trace_experimental_regexp_engine) {
    String source = String::cast(regexp.DataAt(JSRegExp::kSourceIndex));
    StdoutStream{} << "Executing experimental regexp " << source << std::endl;
  }

  Vector<RegExpInstruction> bytecode = AsInstructionSequence(
      ByteArray::cast(regexp.DataAt(JSRegExp::kIrregexpLatin1BytecodeIndex)));

  if (FLAG_print_regexp_bytecode) {
    StdoutStream{} << "Bytecode:" << std::endl;
    StdoutStream{} << bytecode << std::endl;
  }

  DCHECK(subject.IsFlat());
  String::FlatContent subject_content = subject.GetFlatContent(no_gc);

  DCHECK_EQ(output_register_count % 2, 0);
  MatchRange* matches = reinterpret_cast<MatchRange*>(output_registers);
  const int32_t max_match_num = output_register_count / 2;

  if (subject_content.IsOneByte()) {
    return ExperimentalRegExpInterpreter::FindMatchesNfaOneByte(
        bytecode, subject_content.ToOneByteVector(), subject_index, matches,
        max_match_num);
  } else {
    return ExperimentalRegExpInterpreter::FindMatchesNfaTwoByte(
        bytecode, subject_content.ToUC16Vector(), subject_index, matches,
        max_match_num);
  }
}

int32_t ExperimentalRegExp::MatchForCallFromJs(
    Address subject, int32_t start_position, Address input_start,
    Address input_end, int* output_registers, int32_t output_register_count,
    Address backtrack_stack, RegExp::CallOrigin call_origin, Isolate* isolate,
    Address regexp) {
  DCHECK(FLAG_enable_experimental_regexp_engine);

  DCHECK_NOT_NULL(isolate);
  DCHECK_NOT_NULL(output_registers);
  DCHECK(call_origin == RegExp::CallOrigin::kFromJs);

  DisallowHeapAllocation no_gc;
  DisallowJavascriptExecution no_js(isolate);
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  String subject_string = String::cast(Object(subject));

  JSRegExp regexp_obj = JSRegExp::cast(Object(regexp));

  return ExecRaw(regexp_obj, subject_string, output_registers,
                 output_register_count, start_position);
}

MaybeHandle<Object> ExperimentalRegExp::Exec(
    Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject,
    int subject_index, Handle<RegExpMatchInfo> last_match_info) {
  DCHECK(FLAG_enable_experimental_regexp_engine);

  DCHECK_EQ(regexp->TypeTag(), JSRegExp::EXPERIMENTAL);
#ifdef VERIFY_HEAP
  regexp->JSRegExpVerify(isolate);
#endif

  if (!IsCompiled(regexp, isolate)) {
    Compile(isolate, regexp);
  }

  DCHECK(IsCompiled(regexp, isolate));

  subject = String::Flatten(isolate, subject);

  MatchRange match;

  int32_t* output_registers = &match.begin;
  int32_t output_register_count = sizeof(MatchRange) / sizeof(int32_t);

  int capture_count = regexp->CaptureCount();

  int num_matches = ExecRaw(*regexp, *subject, output_registers,
                            output_register_count, subject_index);

  if (num_matches == 0) {
    return isolate->factory()->null_value();
  } else {
    DCHECK_EQ(num_matches, 1);
    return RegExp::SetLastMatchInfo(isolate, last_match_info, subject,
                                    capture_count, output_registers);
    return last_match_info;
  }
}

}  // namespace internal
}  // namespace v8
