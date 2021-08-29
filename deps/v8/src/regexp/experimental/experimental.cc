// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/experimental/experimental.h"

#include "src/common/assert-scope.h"
#include "src/objects/js-regexp-inl.h"
#include "src/regexp/experimental/experimental-compiler.h"
#include "src/regexp/experimental/experimental-interpreter.h"
#include "src/regexp/regexp-parser.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

bool ExperimentalRegExp::CanBeHandled(RegExpTree* tree, JSRegExp::Flags flags,
                                      int capture_count) {
  DCHECK(FLAG_enable_experimental_regexp_engine ||
         FLAG_enable_experimental_regexp_engine_on_excessive_backtracks);
  return ExperimentalRegExpCompiler::CanBeHandled(tree, flags, capture_count);
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

template <class T>
Handle<ByteArray> VectorToByteArray(Isolate* isolate, base::Vector<T> data) {
  STATIC_ASSERT(std::is_trivial<T>::value);

  int byte_length = sizeof(T) * data.length();
  Handle<ByteArray> byte_array = isolate->factory()->NewByteArray(byte_length);
  DisallowGarbageCollection no_gc;
  MemCopy(byte_array->GetDataStartAddress(), data.begin(), byte_length);
  return byte_array;
}

namespace {

struct CompilationResult {
  Handle<ByteArray> bytecode;
  Handle<FixedArray> capture_name_map;
};

// Compiles source pattern, but doesn't change the regexp object.
base::Optional<CompilationResult> CompileImpl(Isolate* isolate,
                                              Handle<JSRegExp> regexp) {
  Zone zone(isolate->allocator(), ZONE_NAME);

  Handle<String> source(regexp->Pattern(), isolate);
  JSRegExp::Flags flags = regexp->GetFlags();

  // Parse and compile the regexp source.
  RegExpCompileData parse_result;
  FlatStringReader reader(isolate, source);
  DCHECK(!isolate->has_pending_exception());

  bool parse_success =
      RegExpParser::ParseRegExp(isolate, &zone, &reader, flags, &parse_result);
  if (!parse_success) {
    // The pattern was already parsed successfully during initialization, so
    // the only way parsing can fail now is because of stack overflow.
    DCHECK_EQ(parse_result.error, RegExpError::kStackOverflow);
    USE(RegExp::ThrowRegExpException(isolate, regexp, source,
                                     parse_result.error));
    return base::nullopt;
  }

  ZoneList<RegExpInstruction> bytecode =
      ExperimentalRegExpCompiler::Compile(parse_result.tree, flags, &zone);

  CompilationResult result;
  result.bytecode = VectorToByteArray(isolate, bytecode.ToVector());
  result.capture_name_map = parse_result.capture_name_map;
  return result;
}

}  // namespace

bool ExperimentalRegExp::Compile(Isolate* isolate, Handle<JSRegExp> re) {
  DCHECK(FLAG_enable_experimental_regexp_engine);
  DCHECK_EQ(re->TypeTag(), JSRegExp::EXPERIMENTAL);
#ifdef VERIFY_HEAP
  re->JSRegExpVerify(isolate);
#endif

  Handle<String> source(re->Pattern(), isolate);
  if (FLAG_trace_experimental_regexp_engine) {
    StdoutStream{} << "Compiling experimental regexp " << *source << std::endl;
  }

  base::Optional<CompilationResult> compilation_result =
      CompileImpl(isolate, re);
  if (!compilation_result.has_value()) {
    DCHECK(isolate->has_pending_exception());
    return false;
  }

  re->SetDataAt(JSRegExp::kIrregexpLatin1BytecodeIndex,
                *compilation_result->bytecode);
  re->SetDataAt(JSRegExp::kIrregexpUC16BytecodeIndex,
                *compilation_result->bytecode);

  Handle<Code> trampoline = BUILTIN_CODE(isolate, RegExpExperimentalTrampoline);
  re->SetDataAt(JSRegExp::kIrregexpLatin1CodeIndex, ToCodeT(*trampoline));
  re->SetDataAt(JSRegExp::kIrregexpUC16CodeIndex, ToCodeT(*trampoline));

  re->SetCaptureNameMap(compilation_result->capture_name_map);

  return true;
}

base::Vector<RegExpInstruction> AsInstructionSequence(ByteArray raw_bytes) {
  RegExpInstruction* inst_begin =
      reinterpret_cast<RegExpInstruction*>(raw_bytes.GetDataStartAddress());
  int inst_num = raw_bytes.length() / sizeof(RegExpInstruction);
  DCHECK_EQ(sizeof(RegExpInstruction) * inst_num, raw_bytes.length());
  return base::Vector<RegExpInstruction>(inst_begin, inst_num);
}

namespace {

int32_t ExecRawImpl(Isolate* isolate, RegExp::CallOrigin call_origin,
                    ByteArray bytecode, String subject, int capture_count,
                    int32_t* output_registers, int32_t output_register_count,
                    int32_t subject_index) {
  DisallowGarbageCollection no_gc;
  // TODO(cbruni): remove once gcmole is fixed.
  DisableGCMole no_gc_mole;

  int register_count_per_match =
      JSRegExp::RegistersForCaptureCount(capture_count);

  int32_t result;
  do {
    DCHECK(subject.IsFlat());
    Zone zone(isolate->allocator(), ZONE_NAME);
    result = ExperimentalRegExpInterpreter::FindMatches(
        isolate, call_origin, bytecode, register_count_per_match, subject,
        subject_index, output_registers, output_register_count, &zone);
  } while (result == RegExp::kInternalRegExpRetry &&
           call_origin == RegExp::kFromRuntime);
  return result;
}

}  // namespace

// Returns the number of matches.
int32_t ExperimentalRegExp::ExecRaw(Isolate* isolate,
                                    RegExp::CallOrigin call_origin,
                                    JSRegExp regexp, String subject,
                                    int32_t* output_registers,
                                    int32_t output_register_count,
                                    int32_t subject_index) {
  DCHECK(FLAG_enable_experimental_regexp_engine);
  DisallowGarbageCollection no_gc;

  if (FLAG_trace_experimental_regexp_engine) {
    String source = String::cast(regexp.DataAt(JSRegExp::kSourceIndex));
    StdoutStream{} << "Executing experimental regexp " << source << std::endl;
  }

  ByteArray bytecode =
      ByteArray::cast(regexp.DataAt(JSRegExp::kIrregexpLatin1BytecodeIndex));

  return ExecRawImpl(isolate, call_origin, bytecode, subject,
                     regexp.CaptureCount(), output_registers,
                     output_register_count, subject_index);
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

  DisallowGarbageCollection no_gc;
  DisallowJavascriptExecution no_js(isolate);
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  String subject_string = String::cast(Object(subject));

  JSRegExp regexp_obj = JSRegExp::cast(Object(regexp));

  return ExecRaw(isolate, RegExp::kFromJs, regexp_obj, subject_string,
                 output_registers, output_register_count, start_position);
}

MaybeHandle<Object> ExperimentalRegExp::Exec(
    Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject,
    int subject_index, Handle<RegExpMatchInfo> last_match_info,
    RegExp::ExecQuirks exec_quirks) {
  DCHECK(FLAG_enable_experimental_regexp_engine);
  DCHECK_EQ(regexp->TypeTag(), JSRegExp::EXPERIMENTAL);
#ifdef VERIFY_HEAP
  regexp->JSRegExpVerify(isolate);
#endif

  if (!IsCompiled(regexp, isolate) && !Compile(isolate, regexp)) {
    DCHECK(isolate->has_pending_exception());
    return MaybeHandle<Object>();
  }

  DCHECK(IsCompiled(regexp, isolate));

  subject = String::Flatten(isolate, subject);

  int capture_count = regexp->CaptureCount();
  int output_register_count = JSRegExp::RegistersForCaptureCount(capture_count);

  int32_t* output_registers;
  std::unique_ptr<int32_t[]> output_registers_release;
  if (output_register_count <= Isolate::kJSRegexpStaticOffsetsVectorSize) {
    output_registers = isolate->jsregexp_static_offsets_vector();
  } else {
    output_registers = NewArray<int32_t>(output_register_count);
    output_registers_release.reset(output_registers);
  }

  int num_matches =
      ExecRaw(isolate, RegExp::kFromRuntime, *regexp, *subject,
              output_registers, output_register_count, subject_index);

  if (num_matches > 0) {
    DCHECK_EQ(num_matches, 1);
    if (exec_quirks == RegExp::ExecQuirks::kTreatMatchAtEndAsFailure) {
      if (output_registers[0] >= subject->length()) {
        return isolate->factory()->null_value();
      }
    }
    return RegExp::SetLastMatchInfo(isolate, last_match_info, subject,
                                    capture_count, output_registers);
  } else if (num_matches == 0) {
    return isolate->factory()->null_value();
  } else {
    DCHECK_LT(num_matches, 0);
    DCHECK(isolate->has_pending_exception());
    return MaybeHandle<Object>();
  }
}

int32_t ExperimentalRegExp::OneshotExecRaw(Isolate* isolate,
                                           Handle<JSRegExp> regexp,
                                           Handle<String> subject,
                                           int32_t* output_registers,
                                           int32_t output_register_count,
                                           int32_t subject_index) {
  DCHECK(FLAG_enable_experimental_regexp_engine_on_excessive_backtracks);

  if (FLAG_trace_experimental_regexp_engine) {
    StdoutStream{} << "Experimental execution (oneshot) of regexp "
                   << regexp->Pattern() << std::endl;
  }

  base::Optional<CompilationResult> compilation_result =
      CompileImpl(isolate, regexp);
  if (!compilation_result.has_value()) return RegExp::kInternalRegExpException;

  DisallowGarbageCollection no_gc;
  return ExecRawImpl(isolate, RegExp::kFromRuntime,
                     *compilation_result->bytecode, *subject,
                     regexp->CaptureCount(), output_registers,
                     output_register_count, subject_index);
}

MaybeHandle<Object> ExperimentalRegExp::OneshotExec(
    Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject,
    int subject_index, Handle<RegExpMatchInfo> last_match_info,
    RegExp::ExecQuirks exec_quirks) {
  DCHECK(FLAG_enable_experimental_regexp_engine_on_excessive_backtracks);
  DCHECK_NE(regexp->TypeTag(), JSRegExp::NOT_COMPILED);

  int capture_count = regexp->CaptureCount();
  int output_register_count = JSRegExp::RegistersForCaptureCount(capture_count);

  int32_t* output_registers;
  std::unique_ptr<int32_t[]> output_registers_release;
  if (output_register_count <= Isolate::kJSRegexpStaticOffsetsVectorSize) {
    output_registers = isolate->jsregexp_static_offsets_vector();
  } else {
    output_registers = NewArray<int32_t>(output_register_count);
    output_registers_release.reset(output_registers);
  }

  int num_matches = OneshotExecRaw(isolate, regexp, subject, output_registers,
                                   output_register_count, subject_index);

  if (num_matches > 0) {
    DCHECK_EQ(num_matches, 1);
    if (exec_quirks == RegExp::ExecQuirks::kTreatMatchAtEndAsFailure) {
      if (output_registers[0] >= subject->length()) {
        return isolate->factory()->null_value();
      }
    }
    return RegExp::SetLastMatchInfo(isolate, last_match_info, subject,
                                    capture_count, output_registers);
  } else if (num_matches == 0) {
    return isolate->factory()->null_value();
  } else {
    DCHECK_LT(num_matches, 0);
    DCHECK(isolate->has_pending_exception());
    return MaybeHandle<Object>();
  }
}

}  // namespace internal
}  // namespace v8
