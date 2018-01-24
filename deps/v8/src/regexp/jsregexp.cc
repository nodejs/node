// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/jsregexp.h"

#include <memory>
#include <vector>

#include "src/base/platform/platform.h"
#include "src/compilation-cache.h"
#include "src/elements.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/ostreams.h"
#include "src/regexp/interpreter-irregexp.h"
#include "src/regexp/jsregexp-inl.h"
#include "src/regexp/regexp-macro-assembler-irregexp.h"
#include "src/regexp/regexp-macro-assembler-tracer.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp-parser.h"
#include "src/regexp/regexp-stack.h"
#include "src/runtime/runtime.h"
#include "src/splay-tree-inl.h"
#include "src/string-search.h"
#include "src/unicode-decoder.h"
#include "src/unicode-inl.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/uniset.h"
#include "unicode/utypes.h"
#endif  // V8_INTL_SUPPORT

#ifndef V8_INTERPRETED_REGEXP
#if V8_TARGET_ARCH_IA32
#include "src/regexp/ia32/regexp-macro-assembler-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/regexp/x64/regexp-macro-assembler-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/regexp/arm64/regexp-macro-assembler-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/regexp/arm/regexp-macro-assembler-arm.h"
#elif V8_TARGET_ARCH_PPC
#include "src/regexp/ppc/regexp-macro-assembler-ppc.h"
#elif V8_TARGET_ARCH_S390
#include "src/regexp/s390/regexp-macro-assembler-s390.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/regexp/mips/regexp-macro-assembler-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/regexp/mips64/regexp-macro-assembler-mips64.h"
#else
#error Unsupported target architecture.
#endif
#endif


namespace v8 {
namespace internal {

MUST_USE_RESULT
static inline MaybeHandle<Object> ThrowRegExpException(
    Handle<JSRegExp> re, Handle<String> pattern, Handle<String> error_text) {
  Isolate* isolate = re->GetIsolate();
  THROW_NEW_ERROR(isolate, NewSyntaxError(MessageTemplate::kMalformedRegExp,
                                          pattern, error_text),
                  Object);
}


inline void ThrowRegExpException(Handle<JSRegExp> re,
                                 Handle<String> error_text) {
  USE(ThrowRegExpException(re, Handle<String>(re->Pattern()), error_text));
}


ContainedInLattice AddRange(ContainedInLattice containment,
                            const int* ranges,
                            int ranges_length,
                            Interval new_range) {
  DCHECK_EQ(1, ranges_length & 1);
  DCHECK_EQ(String::kMaxCodePoint + 1, ranges[ranges_length - 1]);
  if (containment == kLatticeUnknown) return containment;
  bool inside = false;
  int last = 0;
  for (int i = 0; i < ranges_length; inside = !inside, last = ranges[i], i++) {
    // Consider the range from last to ranges[i].
    // We haven't got to the new range yet.
    if (ranges[i] <= new_range.from()) continue;
    // New range is wholly inside last-ranges[i].  Note that new_range.to() is
    // inclusive, but the values in ranges are not.
    if (last <= new_range.from() && new_range.to() < ranges[i]) {
      return Combine(containment, inside ? kLatticeIn : kLatticeOut);
    }
    return kLatticeUnknown;
  }
  return containment;
}

// Generic RegExp methods. Dispatches to implementation specific methods.

// In a 3-character pattern you can maximally step forwards 3 characters
// at a time, which is not always enough to pay for the extra logic.
const int kPatternTooShortForBoyerMoore = 2;

MaybeHandle<Object> RegExpImpl::Compile(Handle<JSRegExp> re,
                                        Handle<String> pattern,
                                        JSRegExp::Flags flags) {
  DCHECK(pattern->IsFlat());

  Isolate* isolate = re->GetIsolate();
  Zone zone(isolate->allocator(), ZONE_NAME);
  CompilationCache* compilation_cache = isolate->compilation_cache();
  MaybeHandle<FixedArray> maybe_cached =
      compilation_cache->LookupRegExp(pattern, flags);
  Handle<FixedArray> cached;
  if (maybe_cached.ToHandle(&cached)) {
    re->set_data(*cached);
    return re;
  }

  PostponeInterruptsScope postpone(isolate);
  RegExpCompileData parse_result;
  FlatStringReader reader(isolate, pattern);
  DCHECK(!isolate->has_pending_exception());
  if (!RegExpParser::ParseRegExp(isolate, &zone, &reader, flags,
                                 &parse_result)) {
    // Throw an exception if we fail to parse the pattern.
    return ThrowRegExpException(re, pattern, parse_result.error);
  }

  bool has_been_compiled = false;

  if (parse_result.simple && !IgnoreCase(flags) && !IsSticky(flags) &&
      pattern->length() <= kPatternTooShortForBoyerMoore) {
    // Parse-tree is a single atom that is equal to the pattern.
    AtomCompile(re, pattern, flags, pattern);
    has_been_compiled = true;
  } else if (parse_result.tree->IsAtom() && !IsSticky(flags) &&
             parse_result.capture_count == 0) {
    RegExpAtom* atom = parse_result.tree->AsAtom();
    Vector<const uc16> atom_pattern = atom->data();
    if (!IgnoreCase(atom->flags()) &&
        atom_pattern.length() <= kPatternTooShortForBoyerMoore) {
      Handle<String> atom_string;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, atom_string,
          isolate->factory()->NewStringFromTwoByte(atom_pattern), Object);
      AtomCompile(re, pattern, flags, atom_string);
      has_been_compiled = true;
    }
  }
  if (!has_been_compiled) {
    IrregexpInitialize(re, pattern, flags, parse_result.capture_count);
  }
  DCHECK(re->data()->IsFixedArray());
  // Compilation succeeded so the data is set on the regexp
  // and we can store it in the cache.
  Handle<FixedArray> data(FixedArray::cast(re->data()));
  compilation_cache->PutRegExp(pattern, flags, data);

  return re;
}

MaybeHandle<Object> RegExpImpl::Exec(Handle<JSRegExp> regexp,
                                     Handle<String> subject, int index,
                                     Handle<RegExpMatchInfo> last_match_info) {
  switch (regexp->TypeTag()) {
    case JSRegExp::ATOM:
      return AtomExec(regexp, subject, index, last_match_info);
    case JSRegExp::IRREGEXP: {
      return IrregexpExec(regexp, subject, index, last_match_info);
    }
    default:
      UNREACHABLE();
  }
}


// RegExp Atom implementation: Simple string search using indexOf.


void RegExpImpl::AtomCompile(Handle<JSRegExp> re,
                             Handle<String> pattern,
                             JSRegExp::Flags flags,
                             Handle<String> match_pattern) {
  re->GetIsolate()->factory()->SetRegExpAtomData(re,
                                                 JSRegExp::ATOM,
                                                 pattern,
                                                 flags,
                                                 match_pattern);
}

static void SetAtomLastCapture(Handle<RegExpMatchInfo> last_match_info,
                               String* subject, int from, int to) {
  SealHandleScope shs(last_match_info->GetIsolate());
  last_match_info->SetNumberOfCaptureRegisters(2);
  last_match_info->SetLastSubject(subject);
  last_match_info->SetLastInput(subject);
  last_match_info->SetCapture(0, from);
  last_match_info->SetCapture(1, to);
}


int RegExpImpl::AtomExecRaw(Handle<JSRegExp> regexp,
                            Handle<String> subject,
                            int index,
                            int32_t* output,
                            int output_size) {
  Isolate* isolate = regexp->GetIsolate();

  DCHECK_LE(0, index);
  DCHECK_LE(index, subject->length());

  subject = String::Flatten(subject);
  DisallowHeapAllocation no_gc;  // ensure vectors stay valid

  String* needle = String::cast(regexp->DataAt(JSRegExp::kAtomPatternIndex));
  int needle_len = needle->length();
  DCHECK(needle->IsFlat());
  DCHECK_LT(0, needle_len);

  if (index + needle_len > subject->length()) {
    return RegExpImpl::RE_FAILURE;
  }

  for (int i = 0; i < output_size; i += 2) {
    String::FlatContent needle_content = needle->GetFlatContent();
    String::FlatContent subject_content = subject->GetFlatContent();
    DCHECK(needle_content.IsFlat());
    DCHECK(subject_content.IsFlat());
    // dispatch on type of strings
    index =
        (needle_content.IsOneByte()
             ? (subject_content.IsOneByte()
                    ? SearchString(isolate, subject_content.ToOneByteVector(),
                                   needle_content.ToOneByteVector(), index)
                    : SearchString(isolate, subject_content.ToUC16Vector(),
                                   needle_content.ToOneByteVector(), index))
             : (subject_content.IsOneByte()
                    ? SearchString(isolate, subject_content.ToOneByteVector(),
                                   needle_content.ToUC16Vector(), index)
                    : SearchString(isolate, subject_content.ToUC16Vector(),
                                   needle_content.ToUC16Vector(), index)));
    if (index == -1) {
      return i / 2;  // Return number of matches.
    } else {
      output[i] = index;
      output[i+1] = index + needle_len;
      index += needle_len;
    }
  }
  return output_size / 2;
}

Handle<Object> RegExpImpl::AtomExec(Handle<JSRegExp> re, Handle<String> subject,
                                    int index,
                                    Handle<RegExpMatchInfo> last_match_info) {
  Isolate* isolate = re->GetIsolate();

  static const int kNumRegisters = 2;
  STATIC_ASSERT(kNumRegisters <= Isolate::kJSRegexpStaticOffsetsVectorSize);
  int32_t* output_registers = isolate->jsregexp_static_offsets_vector();

  int res = AtomExecRaw(re, subject, index, output_registers, kNumRegisters);

  if (res == RegExpImpl::RE_FAILURE) return isolate->factory()->null_value();

  DCHECK_EQ(res, RegExpImpl::RE_SUCCESS);
  SealHandleScope shs(isolate);
  SetAtomLastCapture(last_match_info, *subject, output_registers[0],
                     output_registers[1]);
  return last_match_info;
}


// Irregexp implementation.

// Ensures that the regexp object contains a compiled version of the
// source for either one-byte or two-byte subject strings.
// If the compiled version doesn't already exist, it is compiled
// from the source pattern.
// If compilation fails, an exception is thrown and this function
// returns false.
bool RegExpImpl::EnsureCompiledIrregexp(Handle<JSRegExp> re,
                                        Handle<String> sample_subject,
                                        bool is_one_byte) {
  Object* compiled_code = re->DataAt(JSRegExp::code_index(is_one_byte));
#ifdef V8_INTERPRETED_REGEXP
  if (compiled_code->IsByteArray()) return true;
#else  // V8_INTERPRETED_REGEXP (RegExp native code)
  if (compiled_code->IsCode()) return true;
#endif
  return CompileIrregexp(re, sample_subject, is_one_byte);
}


bool RegExpImpl::CompileIrregexp(Handle<JSRegExp> re,
                                 Handle<String> sample_subject,
                                 bool is_one_byte) {
  // Compile the RegExp.
  Isolate* isolate = re->GetIsolate();
  Zone zone(isolate->allocator(), ZONE_NAME);
  PostponeInterruptsScope postpone(isolate);
#ifdef DEBUG
  Object* entry = re->DataAt(JSRegExp::code_index(is_one_byte));
  // When arriving here entry can only be a smi representing an uncompiled
  // regexp.
  DCHECK(entry->IsSmi());
  int entry_value = Smi::ToInt(entry);
  DCHECK_EQ(JSRegExp::kUninitializedValue, entry_value);
#endif

  JSRegExp::Flags flags = re->GetFlags();

  Handle<String> pattern(re->Pattern());
  pattern = String::Flatten(pattern);
  RegExpCompileData compile_data;
  FlatStringReader reader(isolate, pattern);
  if (!RegExpParser::ParseRegExp(isolate, &zone, &reader, flags,
                                 &compile_data)) {
    // Throw an exception if we fail to parse the pattern.
    // THIS SHOULD NOT HAPPEN. We already pre-parsed it successfully once.
    USE(ThrowRegExpException(re, pattern, compile_data.error));
    return false;
  }
  RegExpEngine::CompilationResult result =
      RegExpEngine::Compile(isolate, &zone, &compile_data, flags, pattern,
                            sample_subject, is_one_byte);
  if (result.error_message != nullptr) {
    // Unable to compile regexp.
    if (FLAG_abort_on_stack_or_string_length_overflow &&
        strncmp(result.error_message, "Stack overflow", 15) == 0) {
      FATAL("Aborting on stack overflow");
    }
    Handle<String> error_message = isolate->factory()->NewStringFromUtf8(
        CStrVector(result.error_message)).ToHandleChecked();
    ThrowRegExpException(re, error_message);
    return false;
  }

  Handle<FixedArray> data = Handle<FixedArray>(FixedArray::cast(re->data()));
  data->set(JSRegExp::code_index(is_one_byte), result.code);
  SetIrregexpCaptureNameMap(*data, compile_data.capture_name_map);
  int register_max = IrregexpMaxRegisterCount(*data);
  if (result.num_registers > register_max) {
    SetIrregexpMaxRegisterCount(*data, result.num_registers);
  }

  return true;
}


int RegExpImpl::IrregexpMaxRegisterCount(FixedArray* re) {
  return Smi::cast(
      re->get(JSRegExp::kIrregexpMaxRegisterCountIndex))->value();
}


void RegExpImpl::SetIrregexpMaxRegisterCount(FixedArray* re, int value) {
  re->set(JSRegExp::kIrregexpMaxRegisterCountIndex, Smi::FromInt(value));
}

void RegExpImpl::SetIrregexpCaptureNameMap(FixedArray* re,
                                           Handle<FixedArray> value) {
  if (value.is_null()) {
    re->set(JSRegExp::kIrregexpCaptureNameMapIndex, Smi::kZero);
  } else {
    re->set(JSRegExp::kIrregexpCaptureNameMapIndex, *value);
  }
}

int RegExpImpl::IrregexpNumberOfCaptures(FixedArray* re) {
  return Smi::ToInt(re->get(JSRegExp::kIrregexpCaptureCountIndex));
}


int RegExpImpl::IrregexpNumberOfRegisters(FixedArray* re) {
  return Smi::ToInt(re->get(JSRegExp::kIrregexpMaxRegisterCountIndex));
}


ByteArray* RegExpImpl::IrregexpByteCode(FixedArray* re, bool is_one_byte) {
  return ByteArray::cast(re->get(JSRegExp::code_index(is_one_byte)));
}


Code* RegExpImpl::IrregexpNativeCode(FixedArray* re, bool is_one_byte) {
  return Code::cast(re->get(JSRegExp::code_index(is_one_byte)));
}


void RegExpImpl::IrregexpInitialize(Handle<JSRegExp> re,
                                    Handle<String> pattern,
                                    JSRegExp::Flags flags,
                                    int capture_count) {
  // Initialize compiled code entries to null.
  re->GetIsolate()->factory()->SetRegExpIrregexpData(re,
                                                     JSRegExp::IRREGEXP,
                                                     pattern,
                                                     flags,
                                                     capture_count);
}


int RegExpImpl::IrregexpPrepare(Handle<JSRegExp> regexp,
                                Handle<String> subject) {
  DCHECK(subject->IsFlat());

  // Check representation of the underlying storage.
  bool is_one_byte = subject->IsOneByteRepresentationUnderneath();
  if (!EnsureCompiledIrregexp(regexp, subject, is_one_byte)) return -1;

#ifdef V8_INTERPRETED_REGEXP
  // Byte-code regexp needs space allocated for all its registers.
  // The result captures are copied to the start of the registers array
  // if the match succeeds.  This way those registers are not clobbered
  // when we set the last match info from last successful match.
  return IrregexpNumberOfRegisters(FixedArray::cast(regexp->data())) +
         (IrregexpNumberOfCaptures(FixedArray::cast(regexp->data())) + 1) * 2;
#else  // V8_INTERPRETED_REGEXP
  // Native regexp only needs room to output captures. Registers are handled
  // internally.
  return (IrregexpNumberOfCaptures(FixedArray::cast(regexp->data())) + 1) * 2;
#endif  // V8_INTERPRETED_REGEXP
}


int RegExpImpl::IrregexpExecRaw(Handle<JSRegExp> regexp,
                                Handle<String> subject,
                                int index,
                                int32_t* output,
                                int output_size) {
  Isolate* isolate = regexp->GetIsolate();

  Handle<FixedArray> irregexp(FixedArray::cast(regexp->data()), isolate);

  DCHECK_LE(0, index);
  DCHECK_LE(index, subject->length());
  DCHECK(subject->IsFlat());

  bool is_one_byte = subject->IsOneByteRepresentationUnderneath();

#ifndef V8_INTERPRETED_REGEXP
  DCHECK(output_size >= (IrregexpNumberOfCaptures(*irregexp) + 1) * 2);
  do {
    EnsureCompiledIrregexp(regexp, subject, is_one_byte);
    Handle<Code> code(IrregexpNativeCode(*irregexp, is_one_byte), isolate);
    // The stack is used to allocate registers for the compiled regexp code.
    // This means that in case of failure, the output registers array is left
    // untouched and contains the capture results from the previous successful
    // match.  We can use that to set the last match info lazily.
    NativeRegExpMacroAssembler::Result res =
        NativeRegExpMacroAssembler::Match(code,
                                          subject,
                                          output,
                                          output_size,
                                          index,
                                          isolate);
    if (res != NativeRegExpMacroAssembler::RETRY) {
      DCHECK(res != NativeRegExpMacroAssembler::EXCEPTION ||
             isolate->has_pending_exception());
      STATIC_ASSERT(
          static_cast<int>(NativeRegExpMacroAssembler::SUCCESS) == RE_SUCCESS);
      STATIC_ASSERT(
          static_cast<int>(NativeRegExpMacroAssembler::FAILURE) == RE_FAILURE);
      STATIC_ASSERT(static_cast<int>(NativeRegExpMacroAssembler::EXCEPTION)
                    == RE_EXCEPTION);
      return static_cast<IrregexpResult>(res);
    }
    // If result is RETRY, the string has changed representation, and we
    // must restart from scratch.
    // In this case, it means we must make sure we are prepared to handle
    // the, potentially, different subject (the string can switch between
    // being internal and external, and even between being Latin1 and UC16,
    // but the characters are always the same).
    IrregexpPrepare(regexp, subject);
    is_one_byte = subject->IsOneByteRepresentationUnderneath();
  } while (true);
  UNREACHABLE();
#else  // V8_INTERPRETED_REGEXP

  DCHECK(output_size >= IrregexpNumberOfRegisters(*irregexp));
  // We must have done EnsureCompiledIrregexp, so we can get the number of
  // registers.
  int number_of_capture_registers =
      (IrregexpNumberOfCaptures(*irregexp) + 1) * 2;
  int32_t* raw_output = &output[number_of_capture_registers];
  // We do not touch the actual capture result registers until we know there
  // has been a match so that we can use those capture results to set the
  // last match info.
  for (int i = number_of_capture_registers - 1; i >= 0; i--) {
    raw_output[i] = -1;
  }
  Handle<ByteArray> byte_codes(IrregexpByteCode(*irregexp, is_one_byte),
                               isolate);

  IrregexpResult result = IrregexpInterpreter::Match(isolate,
                                                     byte_codes,
                                                     subject,
                                                     raw_output,
                                                     index);
  if (result == RE_SUCCESS) {
    // Copy capture results to the start of the registers array.
    MemCopy(output, raw_output, number_of_capture_registers * sizeof(int32_t));
  }
  if (result == RE_EXCEPTION) {
    DCHECK(!isolate->has_pending_exception());
    isolate->StackOverflow();
  }
  return result;
#endif  // V8_INTERPRETED_REGEXP
}

MaybeHandle<Object> RegExpImpl::IrregexpExec(
    Handle<JSRegExp> regexp, Handle<String> subject, int previous_index,
    Handle<RegExpMatchInfo> last_match_info) {
  Isolate* isolate = regexp->GetIsolate();
  DCHECK_EQ(regexp->TypeTag(), JSRegExp::IRREGEXP);

  subject = String::Flatten(subject);

  // Prepare space for the return values.
#if defined(V8_INTERPRETED_REGEXP) && defined(DEBUG)
  if (FLAG_trace_regexp_bytecodes) {
    String* pattern = regexp->Pattern();
    PrintF("\n\nRegexp match:   /%s/\n\n", pattern->ToCString().get());
    PrintF("\n\nSubject string: '%s'\n\n", subject->ToCString().get());
  }
#endif
  int required_registers = RegExpImpl::IrregexpPrepare(regexp, subject);
  if (required_registers < 0) {
    // Compiling failed with an exception.
    DCHECK(isolate->has_pending_exception());
    return MaybeHandle<Object>();
  }

  int32_t* output_registers = nullptr;
  if (required_registers > Isolate::kJSRegexpStaticOffsetsVectorSize) {
    output_registers = NewArray<int32_t>(required_registers);
  }
  std::unique_ptr<int32_t[]> auto_release(output_registers);
  if (output_registers == nullptr) {
    output_registers = isolate->jsregexp_static_offsets_vector();
  }

  int res = RegExpImpl::IrregexpExecRaw(
      regexp, subject, previous_index, output_registers, required_registers);
  if (res == RE_SUCCESS) {
    int capture_count =
        IrregexpNumberOfCaptures(FixedArray::cast(regexp->data()));
    return SetLastMatchInfo(
        last_match_info, subject, capture_count, output_registers);
  }
  if (res == RE_EXCEPTION) {
    DCHECK(isolate->has_pending_exception());
    return MaybeHandle<Object>();
  }
  DCHECK(res == RE_FAILURE);
  return isolate->factory()->null_value();
}

Handle<RegExpMatchInfo> RegExpImpl::SetLastMatchInfo(
    Handle<RegExpMatchInfo> last_match_info, Handle<String> subject,
    int capture_count, int32_t* match) {
  // This is the only place where match infos can grow. If, after executing the
  // regexp, RegExpExecStub finds that the match info is too small, it restarts
  // execution in RegExpImpl::Exec, which finally grows the match info right
  // here.

  int capture_register_count = (capture_count + 1) * 2;
  Handle<RegExpMatchInfo> result =
      RegExpMatchInfo::ReserveCaptures(last_match_info, capture_register_count);
  result->SetNumberOfCaptureRegisters(capture_register_count);

  if (*result != *last_match_info) {
    // The match info has been reallocated, update the corresponding reference
    // on the native context.
    Isolate* isolate = last_match_info->GetIsolate();
    if (*last_match_info == *isolate->regexp_last_match_info()) {
      isolate->native_context()->set_regexp_last_match_info(*result);
    } else if (*last_match_info == *isolate->regexp_internal_match_info()) {
      isolate->native_context()->set_regexp_internal_match_info(*result);
    }
  }

  DisallowHeapAllocation no_allocation;
  if (match != nullptr) {
    for (int i = 0; i < capture_register_count; i += 2) {
      result->SetCapture(i, match[i]);
      result->SetCapture(i + 1, match[i + 1]);
    }
  }
  result->SetLastSubject(*subject);
  result->SetLastInput(*subject);
  return result;
}

RegExpImpl::GlobalCache::GlobalCache(Handle<JSRegExp> regexp,
                                     Handle<String> subject, Isolate* isolate)
    : register_array_(nullptr),
      register_array_size_(0),
      regexp_(regexp),
      subject_(subject) {
#ifdef V8_INTERPRETED_REGEXP
  bool interpreted = true;
#else
  bool interpreted = false;
#endif  // V8_INTERPRETED_REGEXP

  if (regexp_->TypeTag() == JSRegExp::ATOM) {
    static const int kAtomRegistersPerMatch = 2;
    registers_per_match_ = kAtomRegistersPerMatch;
    // There is no distinction between interpreted and native for atom regexps.
    interpreted = false;
  } else {
    registers_per_match_ = RegExpImpl::IrregexpPrepare(regexp_, subject_);
    if (registers_per_match_ < 0) {
      num_matches_ = -1;  // Signal exception.
      return;
    }
  }

  DCHECK(IsGlobal(regexp->GetFlags()));
  if (!interpreted) {
    register_array_size_ =
        Max(registers_per_match_, Isolate::kJSRegexpStaticOffsetsVectorSize);
    max_matches_ = register_array_size_ / registers_per_match_;
  } else {
    // Global loop in interpreted regexp is not implemented.  We choose
    // the size of the offsets vector so that it can only store one match.
    register_array_size_ = registers_per_match_;
    max_matches_ = 1;
  }

  if (register_array_size_ > Isolate::kJSRegexpStaticOffsetsVectorSize) {
    register_array_ = NewArray<int32_t>(register_array_size_);
  } else {
    register_array_ = isolate->jsregexp_static_offsets_vector();
  }

  // Set state so that fetching the results the first time triggers a call
  // to the compiled regexp.
  current_match_index_ = max_matches_ - 1;
  num_matches_ = max_matches_;
  DCHECK_LE(2, registers_per_match_);  // Each match has at least one capture.
  DCHECK_GE(register_array_size_, registers_per_match_);
  int32_t* last_match =
      &register_array_[current_match_index_ * registers_per_match_];
  last_match[0] = -1;
  last_match[1] = 0;
}

int RegExpImpl::GlobalCache::AdvanceZeroLength(int last_index) {
  if (IsUnicode(regexp_->GetFlags()) && last_index + 1 < subject_->length() &&
      unibrow::Utf16::IsLeadSurrogate(subject_->Get(last_index)) &&
      unibrow::Utf16::IsTrailSurrogate(subject_->Get(last_index + 1))) {
    // Advance over the surrogate pair.
    return last_index + 2;
  }
  return last_index + 1;
}

// -------------------------------------------------------------------
// Implementation of the Irregexp regular expression engine.
//
// The Irregexp regular expression engine is intended to be a complete
// implementation of ECMAScript regular expressions.  It generates either
// bytecodes or native code.

//   The Irregexp regexp engine is structured in three steps.
//   1) The parser generates an abstract syntax tree.  See ast.cc.
//   2) From the AST a node network is created.  The nodes are all
//      subclasses of RegExpNode.  The nodes represent states when
//      executing a regular expression.  Several optimizations are
//      performed on the node network.
//   3) From the nodes we generate either byte codes or native code
//      that can actually execute the regular expression (perform
//      the search).  The code generation step is described in more
//      detail below.

// Code generation.
//
//   The nodes are divided into four main categories.
//   * Choice nodes
//        These represent places where the regular expression can
//        match in more than one way.  For example on entry to an
//        alternation (foo|bar) or a repetition (*, +, ? or {}).
//   * Action nodes
//        These represent places where some action should be
//        performed.  Examples include recording the current position
//        in the input string to a register (in order to implement
//        captures) or other actions on register for example in order
//        to implement the counters needed for {} repetitions.
//   * Matching nodes
//        These attempt to match some element part of the input string.
//        Examples of elements include character classes, plain strings
//        or back references.
//   * End nodes
//        These are used to implement the actions required on finding
//        a successful match or failing to find a match.
//
//   The code generated (whether as byte codes or native code) maintains
//   some state as it runs.  This consists of the following elements:
//
//   * The capture registers.  Used for string captures.
//   * Other registers.  Used for counters etc.
//   * The current position.
//   * The stack of backtracking information.  Used when a matching node
//     fails to find a match and needs to try an alternative.
//
// Conceptual regular expression execution model:
//
//   There is a simple conceptual model of regular expression execution
//   which will be presented first.  The actual code generated is a more
//   efficient simulation of the simple conceptual model:
//
//   * Choice nodes are implemented as follows:
//     For each choice except the last {
//       push current position
//       push backtrack code location
//       <generate code to test for choice>
//       backtrack code location:
//       pop current position
//     }
//     <generate code to test for last choice>
//
//   * Actions nodes are generated as follows
//     <push affected registers on backtrack stack>
//     <generate code to perform action>
//     push backtrack code location
//     <generate code to test for following nodes>
//     backtrack code location:
//     <pop affected registers to restore their state>
//     <pop backtrack location from stack and go to it>
//
//   * Matching nodes are generated as follows:
//     if input string matches at current position
//       update current position
//       <generate code to test for following nodes>
//     else
//       <pop backtrack location from stack and go to it>
//
//   Thus it can be seen that the current position is saved and restored
//   by the choice nodes, whereas the registers are saved and restored by
//   by the action nodes that manipulate them.
//
//   The other interesting aspect of this model is that nodes are generated
//   at the point where they are needed by a recursive call to Emit().  If
//   the node has already been code generated then the Emit() call will
//   generate a jump to the previously generated code instead.  In order to
//   limit recursion it is possible for the Emit() function to put the node
//   on a work list for later generation and instead generate a jump.  The
//   destination of the jump is resolved later when the code is generated.
//
// Actual regular expression code generation.
//
//   Code generation is actually more complicated than the above.  In order
//   to improve the efficiency of the generated code some optimizations are
//   performed
//
//   * Choice nodes have 1-character lookahead.
//     A choice node looks at the following character and eliminates some of
//     the choices immediately based on that character.  This is not yet
//     implemented.
//   * Simple greedy loops store reduced backtracking information.
//     A quantifier like /.*foo/m will greedily match the whole input.  It will
//     then need to backtrack to a point where it can match "foo".  The naive
//     implementation of this would push each character position onto the
//     backtracking stack, then pop them off one by one.  This would use space
//     proportional to the length of the input string.  However since the "."
//     can only match in one way and always has a constant length (in this case
//     of 1) it suffices to store the current position on the top of the stack
//     once.  Matching now becomes merely incrementing the current position and
//     backtracking becomes decrementing the current position and checking the
//     result against the stored current position.  This is faster and saves
//     space.
//   * The current state is virtualized.
//     This is used to defer expensive operations until it is clear that they
//     are needed and to generate code for a node more than once, allowing
//     specialized an efficient versions of the code to be created. This is
//     explained in the section below.
//
// Execution state virtualization.
//
//   Instead of emitting code, nodes that manipulate the state can record their
//   manipulation in an object called the Trace.  The Trace object can record a
//   current position offset, an optional backtrack code location on the top of
//   the virtualized backtrack stack and some register changes.  When a node is
//   to be emitted it can flush the Trace or update it.  Flushing the Trace
//   will emit code to bring the actual state into line with the virtual state.
//   Avoiding flushing the state can postpone some work (e.g. updates of capture
//   registers).  Postponing work can save time when executing the regular
//   expression since it may be found that the work never has to be done as a
//   failure to match can occur.  In addition it is much faster to jump to a
//   known backtrack code location than it is to pop an unknown backtrack
//   location from the stack and jump there.
//
//   The virtual state found in the Trace affects code generation.  For example
//   the virtual state contains the difference between the actual current
//   position and the virtual current position, and matching code needs to use
//   this offset to attempt a match in the correct location of the input
//   string.  Therefore code generated for a non-trivial trace is specialized
//   to that trace.  The code generator therefore has the ability to generate
//   code for each node several times.  In order to limit the size of the
//   generated code there is an arbitrary limit on how many specialized sets of
//   code may be generated for a given node.  If the limit is reached, the
//   trace is flushed and a generic version of the code for a node is emitted.
//   This is subsequently used for that node.  The code emitted for non-generic
//   trace is not recorded in the node and so it cannot currently be reused in
//   the event that code generation is requested for an identical trace.


void RegExpTree::AppendToText(RegExpText* text, Zone* zone) {
  UNREACHABLE();
}


void RegExpAtom::AppendToText(RegExpText* text, Zone* zone) {
  text->AddElement(TextElement::Atom(this), zone);
}


void RegExpCharacterClass::AppendToText(RegExpText* text, Zone* zone) {
  text->AddElement(TextElement::CharClass(this), zone);
}


void RegExpText::AppendToText(RegExpText* text, Zone* zone) {
  for (int i = 0; i < elements()->length(); i++)
    text->AddElement(elements()->at(i), zone);
}


TextElement TextElement::Atom(RegExpAtom* atom) {
  return TextElement(ATOM, atom);
}


TextElement TextElement::CharClass(RegExpCharacterClass* char_class) {
  return TextElement(CHAR_CLASS, char_class);
}


int TextElement::length() const {
  switch (text_type()) {
    case ATOM:
      return atom()->length();

    case CHAR_CLASS:
      return 1;
  }
  UNREACHABLE();
}


DispatchTable* ChoiceNode::GetTable(bool ignore_case) {
  if (table_ == nullptr) {
    table_ = new(zone()) DispatchTable(zone());
    DispatchTableConstructor cons(table_, ignore_case, zone());
    cons.BuildTable(this);
  }
  return table_;
}


class FrequencyCollator {
 public:
  FrequencyCollator() : total_samples_(0) {
    for (int i = 0; i < RegExpMacroAssembler::kTableSize; i++) {
      frequencies_[i] = CharacterFrequency(i);
    }
  }

  void CountCharacter(int character) {
    int index = (character & RegExpMacroAssembler::kTableMask);
    frequencies_[index].Increment();
    total_samples_++;
  }

  // Does not measure in percent, but rather per-128 (the table size from the
  // regexp macro assembler).
  int Frequency(int in_character) {
    DCHECK((in_character & RegExpMacroAssembler::kTableMask) == in_character);
    if (total_samples_ < 1) return 1;  // Division by zero.
    int freq_in_per128 =
        (frequencies_[in_character].counter() * 128) / total_samples_;
    return freq_in_per128;
  }

 private:
  class CharacterFrequency {
   public:
    CharacterFrequency() : counter_(0), character_(-1) { }
    explicit CharacterFrequency(int character)
        : counter_(0), character_(character) { }

    void Increment() { counter_++; }
    int counter() { return counter_; }
    int character() { return character_; }

   private:
    int counter_;
    int character_;
  };


 private:
  CharacterFrequency frequencies_[RegExpMacroAssembler::kTableSize];
  int total_samples_;
};


class RegExpCompiler {
 public:
  RegExpCompiler(Isolate* isolate, Zone* zone, int capture_count,
                 bool is_one_byte);

  int AllocateRegister() {
    if (next_register_ >= RegExpMacroAssembler::kMaxRegister) {
      reg_exp_too_big_ = true;
      return next_register_;
    }
    return next_register_++;
  }

  // Lookarounds to match lone surrogates for unicode character class matches
  // are never nested. We can therefore reuse registers.
  int UnicodeLookaroundStackRegister() {
    if (unicode_lookaround_stack_register_ == kNoRegister) {
      unicode_lookaround_stack_register_ = AllocateRegister();
    }
    return unicode_lookaround_stack_register_;
  }

  int UnicodeLookaroundPositionRegister() {
    if (unicode_lookaround_position_register_ == kNoRegister) {
      unicode_lookaround_position_register_ = AllocateRegister();
    }
    return unicode_lookaround_position_register_;
  }

  RegExpEngine::CompilationResult Assemble(RegExpMacroAssembler* assembler,
                                           RegExpNode* start,
                                           int capture_count,
                                           Handle<String> pattern);

  inline void AddWork(RegExpNode* node) {
    if (!node->on_work_list() && !node->label()->is_bound()) {
      node->set_on_work_list(true);
      work_list_->push_back(node);
    }
  }

  static const int kImplementationOffset = 0;
  static const int kNumberOfRegistersOffset = 0;
  static const int kCodeOffset = 1;

  RegExpMacroAssembler* macro_assembler() { return macro_assembler_; }
  EndNode* accept() { return accept_; }

  static const int kMaxRecursion = 100;
  inline int recursion_depth() { return recursion_depth_; }
  inline void IncrementRecursionDepth() { recursion_depth_++; }
  inline void DecrementRecursionDepth() { recursion_depth_--; }

  void SetRegExpTooBig() { reg_exp_too_big_ = true; }

  inline bool one_byte() { return one_byte_; }
  inline bool optimize() { return optimize_; }
  inline void set_optimize(bool value) { optimize_ = value; }
  inline bool limiting_recursion() { return limiting_recursion_; }
  inline void set_limiting_recursion(bool value) {
    limiting_recursion_ = value;
  }
  bool read_backward() { return read_backward_; }
  void set_read_backward(bool value) { read_backward_ = value; }
  FrequencyCollator* frequency_collator() { return &frequency_collator_; }

  int current_expansion_factor() { return current_expansion_factor_; }
  void set_current_expansion_factor(int value) {
    current_expansion_factor_ = value;
  }

  Isolate* isolate() const { return isolate_; }
  Zone* zone() const { return zone_; }

  static const int kNoRegister = -1;

 private:
  EndNode* accept_;
  int next_register_;
  int unicode_lookaround_stack_register_;
  int unicode_lookaround_position_register_;
  std::vector<RegExpNode*>* work_list_;
  int recursion_depth_;
  RegExpMacroAssembler* macro_assembler_;
  bool one_byte_;
  bool reg_exp_too_big_;
  bool limiting_recursion_;
  bool optimize_;
  bool read_backward_;
  int current_expansion_factor_;
  FrequencyCollator frequency_collator_;
  Isolate* isolate_;
  Zone* zone_;
};


class RecursionCheck {
 public:
  explicit RecursionCheck(RegExpCompiler* compiler) : compiler_(compiler) {
    compiler->IncrementRecursionDepth();
  }
  ~RecursionCheck() { compiler_->DecrementRecursionDepth(); }
 private:
  RegExpCompiler* compiler_;
};


static RegExpEngine::CompilationResult IrregexpRegExpTooBig(Isolate* isolate) {
  return RegExpEngine::CompilationResult(isolate, "RegExp too big");
}


// Attempts to compile the regexp using an Irregexp code generator.  Returns
// a fixed array or a null handle depending on whether it succeeded.
RegExpCompiler::RegExpCompiler(Isolate* isolate, Zone* zone, int capture_count,
                               bool one_byte)
    : next_register_(2 * (capture_count + 1)),
      unicode_lookaround_stack_register_(kNoRegister),
      unicode_lookaround_position_register_(kNoRegister),
      work_list_(nullptr),
      recursion_depth_(0),
      one_byte_(one_byte),
      reg_exp_too_big_(false),
      limiting_recursion_(false),
      optimize_(FLAG_regexp_optimization),
      read_backward_(false),
      current_expansion_factor_(1),
      frequency_collator_(),
      isolate_(isolate),
      zone_(zone) {
  accept_ = new(zone) EndNode(EndNode::ACCEPT, zone);
  DCHECK_GE(RegExpMacroAssembler::kMaxRegister, next_register_ - 1);
}


RegExpEngine::CompilationResult RegExpCompiler::Assemble(
    RegExpMacroAssembler* macro_assembler,
    RegExpNode* start,
    int capture_count,
    Handle<String> pattern) {
  Isolate* isolate = pattern->GetHeap()->isolate();

#ifdef DEBUG
  if (FLAG_trace_regexp_assembler)
    macro_assembler_ = new RegExpMacroAssemblerTracer(isolate, macro_assembler);
  else
#endif
    macro_assembler_ = macro_assembler;

  std::vector<RegExpNode*> work_list;
  work_list_ = &work_list;
  Label fail;
  macro_assembler_->PushBacktrack(&fail);
  Trace new_trace;
  start->Emit(this, &new_trace);
  macro_assembler_->Bind(&fail);
  macro_assembler_->Fail();
  while (!work_list.empty()) {
    RegExpNode* node = work_list.back();
    work_list.pop_back();
    node->set_on_work_list(false);
    if (!node->label()->is_bound()) node->Emit(this, &new_trace);
  }
  if (reg_exp_too_big_) {
    macro_assembler_->AbortedCodeGeneration();
    return IrregexpRegExpTooBig(isolate_);
  }

  Handle<HeapObject> code = macro_assembler_->GetCode(pattern);
  isolate->IncreaseTotalRegexpCodeGenerated(code->Size());
  work_list_ = nullptr;
#if defined(ENABLE_DISASSEMBLER) && !defined(V8_INTERPRETED_REGEXP)
  if (FLAG_print_code) {
    CodeTracer::Scope trace_scope(isolate->GetCodeTracer());
    OFStream os(trace_scope.file());
    Handle<Code>::cast(code)->Disassemble(pattern->ToCString().get(), os);
  }
#endif
#ifdef DEBUG
  if (FLAG_trace_regexp_assembler) {
    delete macro_assembler_;
  }
#endif
  return RegExpEngine::CompilationResult(*code, next_register_);
}


bool Trace::DeferredAction::Mentions(int that) {
  if (action_type() == ActionNode::CLEAR_CAPTURES) {
    Interval range = static_cast<DeferredClearCaptures*>(this)->range();
    return range.Contains(that);
  } else {
    return reg() == that;
  }
}


bool Trace::mentions_reg(int reg) {
  for (DeferredAction* action = actions_; action != nullptr;
       action = action->next()) {
    if (action->Mentions(reg))
      return true;
  }
  return false;
}


bool Trace::GetStoredPosition(int reg, int* cp_offset) {
  DCHECK_EQ(0, *cp_offset);
  for (DeferredAction* action = actions_; action != nullptr;
       action = action->next()) {
    if (action->Mentions(reg)) {
      if (action->action_type() == ActionNode::STORE_POSITION) {
        *cp_offset = static_cast<DeferredCapture*>(action)->cp_offset();
        return true;
      } else {
        return false;
      }
    }
  }
  return false;
}


int Trace::FindAffectedRegisters(OutSet* affected_registers,
                                 Zone* zone) {
  int max_register = RegExpCompiler::kNoRegister;
  for (DeferredAction* action = actions_; action != nullptr;
       action = action->next()) {
    if (action->action_type() == ActionNode::CLEAR_CAPTURES) {
      Interval range = static_cast<DeferredClearCaptures*>(action)->range();
      for (int i = range.from(); i <= range.to(); i++)
        affected_registers->Set(i, zone);
      if (range.to() > max_register) max_register = range.to();
    } else {
      affected_registers->Set(action->reg(), zone);
      if (action->reg() > max_register) max_register = action->reg();
    }
  }
  return max_register;
}


void Trace::RestoreAffectedRegisters(RegExpMacroAssembler* assembler,
                                     int max_register,
                                     const OutSet& registers_to_pop,
                                     const OutSet& registers_to_clear) {
  for (int reg = max_register; reg >= 0; reg--) {
    if (registers_to_pop.Get(reg)) {
      assembler->PopRegister(reg);
    } else if (registers_to_clear.Get(reg)) {
      int clear_to = reg;
      while (reg > 0 && registers_to_clear.Get(reg - 1)) {
        reg--;
      }
      assembler->ClearRegisters(reg, clear_to);
    }
  }
}


void Trace::PerformDeferredActions(RegExpMacroAssembler* assembler,
                                   int max_register,
                                   const OutSet& affected_registers,
                                   OutSet* registers_to_pop,
                                   OutSet* registers_to_clear,
                                   Zone* zone) {
  // The "+1" is to avoid a push_limit of zero if stack_limit_slack() is 1.
  const int push_limit = (assembler->stack_limit_slack() + 1) / 2;

  // Count pushes performed to force a stack limit check occasionally.
  int pushes = 0;

  for (int reg = 0; reg <= max_register; reg++) {
    if (!affected_registers.Get(reg)) {
      continue;
    }

    // The chronologically first deferred action in the trace
    // is used to infer the action needed to restore a register
    // to its previous state (or not, if it's safe to ignore it).
    enum DeferredActionUndoType { IGNORE, RESTORE, CLEAR };
    DeferredActionUndoType undo_action = IGNORE;

    int value = 0;
    bool absolute = false;
    bool clear = false;
    static const int kNoStore = kMinInt;
    int store_position = kNoStore;
    // This is a little tricky because we are scanning the actions in reverse
    // historical order (newest first).
    for (DeferredAction* action = actions_; action != nullptr;
         action = action->next()) {
      if (action->Mentions(reg)) {
        switch (action->action_type()) {
          case ActionNode::SET_REGISTER: {
            Trace::DeferredSetRegister* psr =
                static_cast<Trace::DeferredSetRegister*>(action);
            if (!absolute) {
              value += psr->value();
              absolute = true;
            }
            // SET_REGISTER is currently only used for newly introduced loop
            // counters. They can have a significant previous value if they
            // occur in a loop. TODO(lrn): Propagate this information, so
            // we can set undo_action to IGNORE if we know there is no value to
            // restore.
            undo_action = RESTORE;
            DCHECK_EQ(store_position, kNoStore);
            DCHECK(!clear);
            break;
          }
          case ActionNode::INCREMENT_REGISTER:
            if (!absolute) {
              value++;
            }
            DCHECK_EQ(store_position, kNoStore);
            DCHECK(!clear);
            undo_action = RESTORE;
            break;
          case ActionNode::STORE_POSITION: {
            Trace::DeferredCapture* pc =
                static_cast<Trace::DeferredCapture*>(action);
            if (!clear && store_position == kNoStore) {
              store_position = pc->cp_offset();
            }

            // For captures we know that stores and clears alternate.
            // Other register, are never cleared, and if the occur
            // inside a loop, they might be assigned more than once.
            if (reg <= 1) {
              // Registers zero and one, aka "capture zero", is
              // always set correctly if we succeed. There is no
              // need to undo a setting on backtrack, because we
              // will set it again or fail.
              undo_action = IGNORE;
            } else {
              undo_action = pc->is_capture() ? CLEAR : RESTORE;
            }
            DCHECK(!absolute);
            DCHECK_EQ(value, 0);
            break;
          }
          case ActionNode::CLEAR_CAPTURES: {
            // Since we're scanning in reverse order, if we've already
            // set the position we have to ignore historically earlier
            // clearing operations.
            if (store_position == kNoStore) {
              clear = true;
            }
            undo_action = RESTORE;
            DCHECK(!absolute);
            DCHECK_EQ(value, 0);
            break;
          }
          default:
            UNREACHABLE();
            break;
        }
      }
    }
    // Prepare for the undo-action (e.g., push if it's going to be popped).
    if (undo_action == RESTORE) {
      pushes++;
      RegExpMacroAssembler::StackCheckFlag stack_check =
          RegExpMacroAssembler::kNoStackLimitCheck;
      if (pushes == push_limit) {
        stack_check = RegExpMacroAssembler::kCheckStackLimit;
        pushes = 0;
      }

      assembler->PushRegister(reg, stack_check);
      registers_to_pop->Set(reg, zone);
    } else if (undo_action == CLEAR) {
      registers_to_clear->Set(reg, zone);
    }
    // Perform the chronologically last action (or accumulated increment)
    // for the register.
    if (store_position != kNoStore) {
      assembler->WriteCurrentPositionToRegister(reg, store_position);
    } else if (clear) {
      assembler->ClearRegisters(reg, reg);
    } else if (absolute) {
      assembler->SetRegister(reg, value);
    } else if (value != 0) {
      assembler->AdvanceRegister(reg, value);
    }
  }
}


// This is called as we come into a loop choice node and some other tricky
// nodes.  It normalizes the state of the code generator to ensure we can
// generate generic code.
void Trace::Flush(RegExpCompiler* compiler, RegExpNode* successor) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();

  DCHECK(!is_trivial());

  if (actions_ == nullptr && backtrack() == nullptr) {
    // Here we just have some deferred cp advances to fix and we are back to
    // a normal situation.  We may also have to forget some information gained
    // through a quick check that was already performed.
    if (cp_offset_ != 0) assembler->AdvanceCurrentPosition(cp_offset_);
    // Create a new trivial state and generate the node with that.
    Trace new_state;
    successor->Emit(compiler, &new_state);
    return;
  }

  // Generate deferred actions here along with code to undo them again.
  OutSet affected_registers;

  if (backtrack() != nullptr) {
    // Here we have a concrete backtrack location.  These are set up by choice
    // nodes and so they indicate that we have a deferred save of the current
    // position which we may need to emit here.
    assembler->PushCurrentPosition();
  }

  int max_register = FindAffectedRegisters(&affected_registers,
                                           compiler->zone());
  OutSet registers_to_pop;
  OutSet registers_to_clear;
  PerformDeferredActions(assembler,
                         max_register,
                         affected_registers,
                         &registers_to_pop,
                         &registers_to_clear,
                         compiler->zone());
  if (cp_offset_ != 0) {
    assembler->AdvanceCurrentPosition(cp_offset_);
  }

  // Create a new trivial state and generate the node with that.
  Label undo;
  assembler->PushBacktrack(&undo);
  if (successor->KeepRecursing(compiler)) {
    Trace new_state;
    successor->Emit(compiler, &new_state);
  } else {
    compiler->AddWork(successor);
    assembler->GoTo(successor->label());
  }

  // On backtrack we need to restore state.
  assembler->Bind(&undo);
  RestoreAffectedRegisters(assembler,
                           max_register,
                           registers_to_pop,
                           registers_to_clear);
  if (backtrack() == nullptr) {
    assembler->Backtrack();
  } else {
    assembler->PopCurrentPosition();
    assembler->GoTo(backtrack());
  }
}


void NegativeSubmatchSuccess::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();

  // Omit flushing the trace. We discard the entire stack frame anyway.

  if (!label()->is_bound()) {
    // We are completely independent of the trace, since we ignore it,
    // so this code can be used as the generic version.
    assembler->Bind(label());
  }

  // Throw away everything on the backtrack stack since the start
  // of the negative submatch and restore the character position.
  assembler->ReadCurrentPositionFromRegister(current_position_register_);
  assembler->ReadStackPointerFromRegister(stack_pointer_register_);
  if (clear_capture_count_ > 0) {
    // Clear any captures that might have been performed during the success
    // of the body of the negative look-ahead.
    int clear_capture_end = clear_capture_start_ + clear_capture_count_ - 1;
    assembler->ClearRegisters(clear_capture_start_, clear_capture_end);
  }
  // Now that we have unwound the stack we find at the top of the stack the
  // backtrack that the BeginSubmatch node got.
  assembler->Backtrack();
}


void EndNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  if (!trace->is_trivial()) {
    trace->Flush(compiler, this);
    return;
  }
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  if (!label()->is_bound()) {
    assembler->Bind(label());
  }
  switch (action_) {
    case ACCEPT:
      assembler->Succeed();
      return;
    case BACKTRACK:
      assembler->GoTo(trace->backtrack());
      return;
    case NEGATIVE_SUBMATCH_SUCCESS:
      // This case is handled in a different virtual method.
      UNREACHABLE();
  }
  UNIMPLEMENTED();
}


void GuardedAlternative::AddGuard(Guard* guard, Zone* zone) {
  if (guards_ == nullptr) guards_ = new (zone) ZoneList<Guard*>(1, zone);
  guards_->Add(guard, zone);
}


ActionNode* ActionNode::SetRegister(int reg,
                                    int val,
                                    RegExpNode* on_success) {
  ActionNode* result =
      new(on_success->zone()) ActionNode(SET_REGISTER, on_success);
  result->data_.u_store_register.reg = reg;
  result->data_.u_store_register.value = val;
  return result;
}


ActionNode* ActionNode::IncrementRegister(int reg, RegExpNode* on_success) {
  ActionNode* result =
      new(on_success->zone()) ActionNode(INCREMENT_REGISTER, on_success);
  result->data_.u_increment_register.reg = reg;
  return result;
}


ActionNode* ActionNode::StorePosition(int reg,
                                      bool is_capture,
                                      RegExpNode* on_success) {
  ActionNode* result =
      new(on_success->zone()) ActionNode(STORE_POSITION, on_success);
  result->data_.u_position_register.reg = reg;
  result->data_.u_position_register.is_capture = is_capture;
  return result;
}


ActionNode* ActionNode::ClearCaptures(Interval range,
                                      RegExpNode* on_success) {
  ActionNode* result =
      new(on_success->zone()) ActionNode(CLEAR_CAPTURES, on_success);
  result->data_.u_clear_captures.range_from = range.from();
  result->data_.u_clear_captures.range_to = range.to();
  return result;
}


ActionNode* ActionNode::BeginSubmatch(int stack_reg,
                                      int position_reg,
                                      RegExpNode* on_success) {
  ActionNode* result =
      new(on_success->zone()) ActionNode(BEGIN_SUBMATCH, on_success);
  result->data_.u_submatch.stack_pointer_register = stack_reg;
  result->data_.u_submatch.current_position_register = position_reg;
  return result;
}


ActionNode* ActionNode::PositiveSubmatchSuccess(int stack_reg,
                                                int position_reg,
                                                int clear_register_count,
                                                int clear_register_from,
                                                RegExpNode* on_success) {
  ActionNode* result =
      new(on_success->zone()) ActionNode(POSITIVE_SUBMATCH_SUCCESS, on_success);
  result->data_.u_submatch.stack_pointer_register = stack_reg;
  result->data_.u_submatch.current_position_register = position_reg;
  result->data_.u_submatch.clear_register_count = clear_register_count;
  result->data_.u_submatch.clear_register_from = clear_register_from;
  return result;
}


ActionNode* ActionNode::EmptyMatchCheck(int start_register,
                                        int repetition_register,
                                        int repetition_limit,
                                        RegExpNode* on_success) {
  ActionNode* result =
      new(on_success->zone()) ActionNode(EMPTY_MATCH_CHECK, on_success);
  result->data_.u_empty_match_check.start_register = start_register;
  result->data_.u_empty_match_check.repetition_register = repetition_register;
  result->data_.u_empty_match_check.repetition_limit = repetition_limit;
  return result;
}


#define DEFINE_ACCEPT(Type)                                          \
  void Type##Node::Accept(NodeVisitor* visitor) {                    \
    visitor->Visit##Type(this);                                      \
  }
FOR_EACH_NODE_TYPE(DEFINE_ACCEPT)
#undef DEFINE_ACCEPT


void LoopChoiceNode::Accept(NodeVisitor* visitor) {
  visitor->VisitLoopChoice(this);
}


// -------------------------------------------------------------------
// Emit code.


void ChoiceNode::GenerateGuard(RegExpMacroAssembler* macro_assembler,
                               Guard* guard,
                               Trace* trace) {
  switch (guard->op()) {
    case Guard::LT:
      DCHECK(!trace->mentions_reg(guard->reg()));
      macro_assembler->IfRegisterGE(guard->reg(),
                                    guard->value(),
                                    trace->backtrack());
      break;
    case Guard::GEQ:
      DCHECK(!trace->mentions_reg(guard->reg()));
      macro_assembler->IfRegisterLT(guard->reg(),
                                    guard->value(),
                                    trace->backtrack());
      break;
  }
}


// Returns the number of characters in the equivalence class, omitting those
// that cannot occur in the source string because it is Latin1.
static int GetCaseIndependentLetters(Isolate* isolate, uc16 character,
                                     bool one_byte_subject,
                                     unibrow::uchar* letters) {
  int length =
      isolate->jsregexp_uncanonicalize()->get(character, '\0', letters);
  // Unibrow returns 0 or 1 for characters where case independence is
  // trivial.
  if (length == 0) {
    letters[0] = character;
    length = 1;
  }

  if (one_byte_subject) {
    int new_length = 0;
    for (int i = 0; i < length; i++) {
      if (letters[i] <= String::kMaxOneByteCharCode) {
        letters[new_length++] = letters[i];
      }
    }
    length = new_length;
  }

  return length;
}


static inline bool EmitSimpleCharacter(Isolate* isolate,
                                       RegExpCompiler* compiler,
                                       uc16 c,
                                       Label* on_failure,
                                       int cp_offset,
                                       bool check,
                                       bool preloaded) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  bool bound_checked = false;
  if (!preloaded) {
    assembler->LoadCurrentCharacter(
        cp_offset,
        on_failure,
        check);
    bound_checked = true;
  }
  assembler->CheckNotCharacter(c, on_failure);
  return bound_checked;
}


// Only emits non-letters (things that don't have case).  Only used for case
// independent matches.
static inline bool EmitAtomNonLetter(Isolate* isolate,
                                     RegExpCompiler* compiler,
                                     uc16 c,
                                     Label* on_failure,
                                     int cp_offset,
                                     bool check,
                                     bool preloaded) {
  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  bool one_byte = compiler->one_byte();
  unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
  int length = GetCaseIndependentLetters(isolate, c, one_byte, chars);
  if (length < 1) {
    // This can't match.  Must be an one-byte subject and a non-one-byte
    // character.  We do not need to do anything since the one-byte pass
    // already handled this.
    return false;  // Bounds not checked.
  }
  bool checked = false;
  // We handle the length > 1 case in a later pass.
  if (length == 1) {
    if (one_byte && c > String::kMaxOneByteCharCodeU) {
      // Can't match - see above.
      return false;  // Bounds not checked.
    }
    if (!preloaded) {
      macro_assembler->LoadCurrentCharacter(cp_offset, on_failure, check);
      checked = check;
    }
    macro_assembler->CheckNotCharacter(c, on_failure);
  }
  return checked;
}


static bool ShortCutEmitCharacterPair(RegExpMacroAssembler* macro_assembler,
                                      bool one_byte, uc16 c1, uc16 c2,
                                      Label* on_failure) {
  uc16 char_mask;
  if (one_byte) {
    char_mask = String::kMaxOneByteCharCode;
  } else {
    char_mask = String::kMaxUtf16CodeUnit;
  }
  uc16 exor = c1 ^ c2;
  // Check whether exor has only one bit set.
  if (((exor - 1) & exor) == 0) {
    // If c1 and c2 differ only by one bit.
    // Ecma262UnCanonicalize always gives the highest number last.
    DCHECK(c2 > c1);
    uc16 mask = char_mask ^ exor;
    macro_assembler->CheckNotCharacterAfterAnd(c1, mask, on_failure);
    return true;
  }
  DCHECK(c2 > c1);
  uc16 diff = c2 - c1;
  if (((diff - 1) & diff) == 0 && c1 >= diff) {
    // If the characters differ by 2^n but don't differ by one bit then
    // subtract the difference from the found character, then do the or
    // trick.  We avoid the theoretical case where negative numbers are
    // involved in order to simplify code generation.
    uc16 mask = char_mask ^ diff;
    macro_assembler->CheckNotCharacterAfterMinusAnd(c1 - diff,
                                                    diff,
                                                    mask,
                                                    on_failure);
    return true;
  }
  return false;
}


typedef bool EmitCharacterFunction(Isolate* isolate,
                                   RegExpCompiler* compiler,
                                   uc16 c,
                                   Label* on_failure,
                                   int cp_offset,
                                   bool check,
                                   bool preloaded);

// Only emits letters (things that have case).  Only used for case independent
// matches.
static inline bool EmitAtomLetter(Isolate* isolate,
                                  RegExpCompiler* compiler,
                                  uc16 c,
                                  Label* on_failure,
                                  int cp_offset,
                                  bool check,
                                  bool preloaded) {
  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  bool one_byte = compiler->one_byte();
  unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
  int length = GetCaseIndependentLetters(isolate, c, one_byte, chars);
  if (length <= 1) return false;
  // We may not need to check against the end of the input string
  // if this character lies before a character that matched.
  if (!preloaded) {
    macro_assembler->LoadCurrentCharacter(cp_offset, on_failure, check);
  }
  Label ok;
  DCHECK_EQ(4, unibrow::Ecma262UnCanonicalize::kMaxWidth);
  switch (length) {
    case 2: {
      if (ShortCutEmitCharacterPair(macro_assembler, one_byte, chars[0],
                                    chars[1], on_failure)) {
      } else {
        macro_assembler->CheckCharacter(chars[0], &ok);
        macro_assembler->CheckNotCharacter(chars[1], on_failure);
        macro_assembler->Bind(&ok);
      }
      break;
    }
    case 4:
      macro_assembler->CheckCharacter(chars[3], &ok);
      // Fall through!
    case 3:
      macro_assembler->CheckCharacter(chars[0], &ok);
      macro_assembler->CheckCharacter(chars[1], &ok);
      macro_assembler->CheckNotCharacter(chars[2], on_failure);
      macro_assembler->Bind(&ok);
      break;
    default:
      UNREACHABLE();
      break;
  }
  return true;
}


static void EmitBoundaryTest(RegExpMacroAssembler* masm,
                             int border,
                             Label* fall_through,
                             Label* above_or_equal,
                             Label* below) {
  if (below != fall_through) {
    masm->CheckCharacterLT(border, below);
    if (above_or_equal != fall_through) masm->GoTo(above_or_equal);
  } else {
    masm->CheckCharacterGT(border - 1, above_or_equal);
  }
}


static void EmitDoubleBoundaryTest(RegExpMacroAssembler* masm,
                                   int first,
                                   int last,
                                   Label* fall_through,
                                   Label* in_range,
                                   Label* out_of_range) {
  if (in_range == fall_through) {
    if (first == last) {
      masm->CheckNotCharacter(first, out_of_range);
    } else {
      masm->CheckCharacterNotInRange(first, last, out_of_range);
    }
  } else {
    if (first == last) {
      masm->CheckCharacter(first, in_range);
    } else {
      masm->CheckCharacterInRange(first, last, in_range);
    }
    if (out_of_range != fall_through) masm->GoTo(out_of_range);
  }
}


// even_label is for ranges[i] to ranges[i + 1] where i - start_index is even.
// odd_label is for ranges[i] to ranges[i + 1] where i - start_index is odd.
static void EmitUseLookupTable(
    RegExpMacroAssembler* masm,
    ZoneList<int>* ranges,
    int start_index,
    int end_index,
    int min_char,
    Label* fall_through,
    Label* even_label,
    Label* odd_label) {
  static const int kSize = RegExpMacroAssembler::kTableSize;
  static const int kMask = RegExpMacroAssembler::kTableMask;

  int base = (min_char & ~kMask);
  USE(base);

  // Assert that everything is on one kTableSize page.
  for (int i = start_index; i <= end_index; i++) {
    DCHECK_EQ(ranges->at(i) & ~kMask, base);
  }
  DCHECK(start_index == 0 || (ranges->at(start_index - 1) & ~kMask) <= base);

  char templ[kSize];
  Label* on_bit_set;
  Label* on_bit_clear;
  int bit;
  if (even_label == fall_through) {
    on_bit_set = odd_label;
    on_bit_clear = even_label;
    bit = 1;
  } else {
    on_bit_set = even_label;
    on_bit_clear = odd_label;
    bit = 0;
  }
  for (int i = 0; i < (ranges->at(start_index) & kMask) && i < kSize; i++) {
    templ[i] = bit;
  }
  int j = 0;
  bit ^= 1;
  for (int i = start_index; i < end_index; i++) {
    for (j = (ranges->at(i) & kMask); j < (ranges->at(i + 1) & kMask); j++) {
      templ[j] = bit;
    }
    bit ^= 1;
  }
  for (int i = j; i < kSize; i++) {
    templ[i] = bit;
  }
  Factory* factory = masm->isolate()->factory();
  // TODO(erikcorry): Cache these.
  Handle<ByteArray> ba = factory->NewByteArray(kSize, TENURED);
  for (int i = 0; i < kSize; i++) {
    ba->set(i, templ[i]);
  }
  masm->CheckBitInTable(ba, on_bit_set);
  if (on_bit_clear != fall_through) masm->GoTo(on_bit_clear);
}


static void CutOutRange(RegExpMacroAssembler* masm,
                        ZoneList<int>* ranges,
                        int start_index,
                        int end_index,
                        int cut_index,
                        Label* even_label,
                        Label* odd_label) {
  bool odd = (((cut_index - start_index) & 1) == 1);
  Label* in_range_label = odd ? odd_label : even_label;
  Label dummy;
  EmitDoubleBoundaryTest(masm,
                         ranges->at(cut_index),
                         ranges->at(cut_index + 1) - 1,
                         &dummy,
                         in_range_label,
                         &dummy);
  DCHECK(!dummy.is_linked());
  // Cut out the single range by rewriting the array.  This creates a new
  // range that is a merger of the two ranges on either side of the one we
  // are cutting out.  The oddity of the labels is preserved.
  for (int j = cut_index; j > start_index; j--) {
    ranges->at(j) = ranges->at(j - 1);
  }
  for (int j = cut_index + 1; j < end_index; j++) {
    ranges->at(j) = ranges->at(j + 1);
  }
}


// Unicode case.  Split the search space into kSize spaces that are handled
// with recursion.
static void SplitSearchSpace(ZoneList<int>* ranges,
                             int start_index,
                             int end_index,
                             int* new_start_index,
                             int* new_end_index,
                             int* border) {
  static const int kSize = RegExpMacroAssembler::kTableSize;
  static const int kMask = RegExpMacroAssembler::kTableMask;

  int first = ranges->at(start_index);
  int last = ranges->at(end_index) - 1;

  *new_start_index = start_index;
  *border = (ranges->at(start_index) & ~kMask) + kSize;
  while (*new_start_index < end_index) {
    if (ranges->at(*new_start_index) > *border) break;
    (*new_start_index)++;
  }
  // new_start_index is the index of the first edge that is beyond the
  // current kSize space.

  // For very large search spaces we do a binary chop search of the non-Latin1
  // space instead of just going to the end of the current kSize space.  The
  // heuristics are complicated a little by the fact that any 128-character
  // encoding space can be quickly tested with a table lookup, so we don't
  // wish to do binary chop search at a smaller granularity than that.  A
  // 128-character space can take up a lot of space in the ranges array if,
  // for example, we only want to match every second character (eg. the lower
  // case characters on some Unicode pages).
  int binary_chop_index = (end_index + start_index) / 2;
  // The first test ensures that we get to the code that handles the Latin1
  // range with a single not-taken branch, speeding up this important
  // character range (even non-Latin1 charset-based text has spaces and
  // punctuation).
  if (*border - 1 > String::kMaxOneByteCharCode &&  // Latin1 case.
      end_index - start_index > (*new_start_index - start_index) * 2 &&
      last - first > kSize * 2 && binary_chop_index > *new_start_index &&
      ranges->at(binary_chop_index) >= first + 2 * kSize) {
    int scan_forward_for_section_border = binary_chop_index;;
    int new_border = (ranges->at(binary_chop_index) | kMask) + 1;

    while (scan_forward_for_section_border < end_index) {
      if (ranges->at(scan_forward_for_section_border) > new_border) {
        *new_start_index = scan_forward_for_section_border;
        *border = new_border;
        break;
      }
      scan_forward_for_section_border++;
    }
  }

  DCHECK(*new_start_index > start_index);
  *new_end_index = *new_start_index - 1;
  if (ranges->at(*new_end_index) == *border) {
    (*new_end_index)--;
  }
  if (*border >= ranges->at(end_index)) {
    *border = ranges->at(end_index);
    *new_start_index = end_index;  // Won't be used.
    *new_end_index = end_index - 1;
  }
}

// Gets a series of segment boundaries representing a character class.  If the
// character is in the range between an even and an odd boundary (counting from
// start_index) then go to even_label, otherwise go to odd_label.  We already
// know that the character is in the range of min_char to max_char inclusive.
// Either label can be nullptr indicating backtracking.  Either label can also
// be equal to the fall_through label.
static void GenerateBranches(RegExpMacroAssembler* masm, ZoneList<int>* ranges,
                             int start_index, int end_index, uc32 min_char,
                             uc32 max_char, Label* fall_through,
                             Label* even_label, Label* odd_label) {
  DCHECK_LE(min_char, String::kMaxUtf16CodeUnit);
  DCHECK_LE(max_char, String::kMaxUtf16CodeUnit);

  int first = ranges->at(start_index);
  int last = ranges->at(end_index) - 1;

  DCHECK_LT(min_char, first);

  // Just need to test if the character is before or on-or-after
  // a particular character.
  if (start_index == end_index) {
    EmitBoundaryTest(masm, first, fall_through, even_label, odd_label);
    return;
  }

  // Another almost trivial case:  There is one interval in the middle that is
  // different from the end intervals.
  if (start_index + 1 == end_index) {
    EmitDoubleBoundaryTest(
        masm, first, last, fall_through, even_label, odd_label);
    return;
  }

  // It's not worth using table lookup if there are very few intervals in the
  // character class.
  if (end_index - start_index <= 6) {
    // It is faster to test for individual characters, so we look for those
    // first, then try arbitrary ranges in the second round.
    static int kNoCutIndex = -1;
    int cut = kNoCutIndex;
    for (int i = start_index; i < end_index; i++) {
      if (ranges->at(i) == ranges->at(i + 1) - 1) {
        cut = i;
        break;
      }
    }
    if (cut == kNoCutIndex) cut = start_index;
    CutOutRange(
        masm, ranges, start_index, end_index, cut, even_label, odd_label);
    DCHECK_GE(end_index - start_index, 2);
    GenerateBranches(masm,
                     ranges,
                     start_index + 1,
                     end_index - 1,
                     min_char,
                     max_char,
                     fall_through,
                     even_label,
                     odd_label);
    return;
  }

  // If there are a lot of intervals in the regexp, then we will use tables to
  // determine whether the character is inside or outside the character class.
  static const int kBits = RegExpMacroAssembler::kTableSizeBits;

  if ((max_char >> kBits) == (min_char >> kBits)) {
    EmitUseLookupTable(masm,
                       ranges,
                       start_index,
                       end_index,
                       min_char,
                       fall_through,
                       even_label,
                       odd_label);
    return;
  }

  if ((min_char >> kBits) != (first >> kBits)) {
    masm->CheckCharacterLT(first, odd_label);
    GenerateBranches(masm,
                     ranges,
                     start_index + 1,
                     end_index,
                     first,
                     max_char,
                     fall_through,
                     odd_label,
                     even_label);
    return;
  }

  int new_start_index = 0;
  int new_end_index = 0;
  int border = 0;

  SplitSearchSpace(ranges,
                   start_index,
                   end_index,
                   &new_start_index,
                   &new_end_index,
                   &border);

  Label handle_rest;
  Label* above = &handle_rest;
  if (border == last + 1) {
    // We didn't find any section that started after the limit, so everything
    // above the border is one of the terminal labels.
    above = (end_index & 1) != (start_index & 1) ? odd_label : even_label;
    DCHECK(new_end_index == end_index - 1);
  }

  DCHECK_LE(start_index, new_end_index);
  DCHECK_LE(new_start_index, end_index);
  DCHECK_LT(start_index, new_start_index);
  DCHECK_LT(new_end_index, end_index);
  DCHECK(new_end_index + 1 == new_start_index ||
         (new_end_index + 2 == new_start_index &&
          border == ranges->at(new_end_index + 1)));
  DCHECK_LT(min_char, border - 1);
  DCHECK_LT(border, max_char);
  DCHECK_LT(ranges->at(new_end_index), border);
  DCHECK(border < ranges->at(new_start_index) ||
         (border == ranges->at(new_start_index) &&
          new_start_index == end_index &&
          new_end_index == end_index - 1 &&
          border == last + 1));
  DCHECK(new_start_index == 0 || border >= ranges->at(new_start_index - 1));

  masm->CheckCharacterGT(border - 1, above);
  Label dummy;
  GenerateBranches(masm,
                   ranges,
                   start_index,
                   new_end_index,
                   min_char,
                   border - 1,
                   &dummy,
                   even_label,
                   odd_label);
  if (handle_rest.is_linked()) {
    masm->Bind(&handle_rest);
    bool flip = (new_start_index & 1) != (start_index & 1);
    GenerateBranches(masm,
                     ranges,
                     new_start_index,
                     end_index,
                     border,
                     max_char,
                     &dummy,
                     flip ? odd_label : even_label,
                     flip ? even_label : odd_label);
  }
}


static void EmitCharClass(RegExpMacroAssembler* macro_assembler,
                          RegExpCharacterClass* cc, bool one_byte,
                          Label* on_failure, int cp_offset, bool check_offset,
                          bool preloaded, Zone* zone) {
  ZoneList<CharacterRange>* ranges = cc->ranges(zone);
  CharacterRange::Canonicalize(ranges);

  int max_char;
  if (one_byte) {
    max_char = String::kMaxOneByteCharCode;
  } else {
    max_char = String::kMaxUtf16CodeUnit;
  }

  int range_count = ranges->length();

  int last_valid_range = range_count - 1;
  while (last_valid_range >= 0) {
    CharacterRange& range = ranges->at(last_valid_range);
    if (range.from() <= max_char) {
      break;
    }
    last_valid_range--;
  }

  if (last_valid_range < 0) {
    if (!cc->is_negated()) {
      macro_assembler->GoTo(on_failure);
    }
    if (check_offset) {
      macro_assembler->CheckPosition(cp_offset, on_failure);
    }
    return;
  }

  if (last_valid_range == 0 &&
      ranges->at(0).IsEverything(max_char)) {
    if (cc->is_negated()) {
      macro_assembler->GoTo(on_failure);
    } else {
      // This is a common case hit by non-anchored expressions.
      if (check_offset) {
        macro_assembler->CheckPosition(cp_offset, on_failure);
      }
    }
    return;
  }

  if (!preloaded) {
    macro_assembler->LoadCurrentCharacter(cp_offset, on_failure, check_offset);
  }

  if (cc->is_standard(zone) &&
      macro_assembler->CheckSpecialCharacterClass(cc->standard_type(),
                                                  on_failure)) {
      return;
  }


  // A new list with ascending entries.  Each entry is a code unit
  // where there is a boundary between code units that are part of
  // the class and code units that are not.  Normally we insert an
  // entry at zero which goes to the failure label, but if there
  // was already one there we fall through for success on that entry.
  // Subsequent entries have alternating meaning (success/failure).
  ZoneList<int>* range_boundaries =
      new(zone) ZoneList<int>(last_valid_range, zone);

  bool zeroth_entry_is_failure = !cc->is_negated();

  for (int i = 0; i <= last_valid_range; i++) {
    CharacterRange& range = ranges->at(i);
    if (range.from() == 0) {
      DCHECK_EQ(i, 0);
      zeroth_entry_is_failure = !zeroth_entry_is_failure;
    } else {
      range_boundaries->Add(range.from(), zone);
    }
    range_boundaries->Add(range.to() + 1, zone);
  }
  int end_index = range_boundaries->length() - 1;
  if (range_boundaries->at(end_index) > max_char) {
    end_index--;
  }

  Label fall_through;
  GenerateBranches(macro_assembler,
                   range_boundaries,
                   0,  // start_index.
                   end_index,
                   0,  // min_char.
                   max_char,
                   &fall_through,
                   zeroth_entry_is_failure ? &fall_through : on_failure,
                   zeroth_entry_is_failure ? on_failure : &fall_through);
  macro_assembler->Bind(&fall_through);
}


RegExpNode::~RegExpNode() {
}


RegExpNode::LimitResult RegExpNode::LimitVersions(RegExpCompiler* compiler,
                                                  Trace* trace) {
  // If we are generating a greedy loop then don't stop and don't reuse code.
  if (trace->stop_node() != nullptr) {
    return CONTINUE;
  }

  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  if (trace->is_trivial()) {
    if (label_.is_bound() || on_work_list() || !KeepRecursing(compiler)) {
      // If a generic version is already scheduled to be generated or we have
      // recursed too deeply then just generate a jump to that code.
      macro_assembler->GoTo(&label_);
      // This will queue it up for generation of a generic version if it hasn't
      // already been queued.
      compiler->AddWork(this);
      return DONE;
    }
    // Generate generic version of the node and bind the label for later use.
    macro_assembler->Bind(&label_);
    return CONTINUE;
  }

  // We are being asked to make a non-generic version.  Keep track of how many
  // non-generic versions we generate so as not to overdo it.
  trace_count_++;
  if (KeepRecursing(compiler) && compiler->optimize() &&
      trace_count_ < kMaxCopiesCodeGenerated) {
    return CONTINUE;
  }

  // If we get here code has been generated for this node too many times or
  // recursion is too deep.  Time to switch to a generic version.  The code for
  // generic versions above can handle deep recursion properly.
  bool was_limiting = compiler->limiting_recursion();
  compiler->set_limiting_recursion(true);
  trace->Flush(compiler, this);
  compiler->set_limiting_recursion(was_limiting);
  return DONE;
}


bool RegExpNode::KeepRecursing(RegExpCompiler* compiler) {
  return !compiler->limiting_recursion() &&
         compiler->recursion_depth() <= RegExpCompiler::kMaxRecursion;
}


int ActionNode::EatsAtLeast(int still_to_find,
                            int budget,
                            bool not_at_start) {
  if (budget <= 0) return 0;
  if (action_type_ == POSITIVE_SUBMATCH_SUCCESS) return 0;  // Rewinds input!
  return on_success()->EatsAtLeast(still_to_find,
                                   budget - 1,
                                   not_at_start);
}


void ActionNode::FillInBMInfo(Isolate* isolate, int offset, int budget,
                              BoyerMooreLookahead* bm, bool not_at_start) {
  if (action_type_ != POSITIVE_SUBMATCH_SUCCESS) {
    on_success()->FillInBMInfo(isolate, offset, budget - 1, bm, not_at_start);
  }
  SaveBMInfo(bm, not_at_start, offset);
}


int AssertionNode::EatsAtLeast(int still_to_find,
                               int budget,
                               bool not_at_start) {
  if (budget <= 0) return 0;
  // If we know we are not at the start and we are asked "how many characters
  // will you match if you succeed?" then we can answer anything since false
  // implies false.  So lets just return the max answer (still_to_find) since
  // that won't prevent us from preloading a lot of characters for the other
  // branches in the node graph.
  if (assertion_type() == AT_START && not_at_start) return still_to_find;
  return on_success()->EatsAtLeast(still_to_find,
                                   budget - 1,
                                   not_at_start);
}


void AssertionNode::FillInBMInfo(Isolate* isolate, int offset, int budget,
                                 BoyerMooreLookahead* bm, bool not_at_start) {
  // Match the behaviour of EatsAtLeast on this node.
  if (assertion_type() == AT_START && not_at_start) return;
  on_success()->FillInBMInfo(isolate, offset, budget - 1, bm, not_at_start);
  SaveBMInfo(bm, not_at_start, offset);
}


int BackReferenceNode::EatsAtLeast(int still_to_find,
                                   int budget,
                                   bool not_at_start) {
  if (read_backward()) return 0;
  if (budget <= 0) return 0;
  return on_success()->EatsAtLeast(still_to_find,
                                   budget - 1,
                                   not_at_start);
}


int TextNode::EatsAtLeast(int still_to_find,
                          int budget,
                          bool not_at_start) {
  if (read_backward()) return 0;
  int answer = Length();
  if (answer >= still_to_find) return answer;
  if (budget <= 0) return answer;
  // We are not at start after this node so we set the last argument to 'true'.
  return answer + on_success()->EatsAtLeast(still_to_find - answer,
                                            budget - 1,
                                            true);
}


int NegativeLookaroundChoiceNode::EatsAtLeast(int still_to_find, int budget,
                                              bool not_at_start) {
  if (budget <= 0) return 0;
  // Alternative 0 is the negative lookahead, alternative 1 is what comes
  // afterwards.
  RegExpNode* node = alternatives_->at(1).node();
  return node->EatsAtLeast(still_to_find, budget - 1, not_at_start);
}


void NegativeLookaroundChoiceNode::GetQuickCheckDetails(
    QuickCheckDetails* details, RegExpCompiler* compiler, int filled_in,
    bool not_at_start) {
  // Alternative 0 is the negative lookahead, alternative 1 is what comes
  // afterwards.
  RegExpNode* node = alternatives_->at(1).node();
  return node->GetQuickCheckDetails(details, compiler, filled_in, not_at_start);
}


int ChoiceNode::EatsAtLeastHelper(int still_to_find,
                                  int budget,
                                  RegExpNode* ignore_this_node,
                                  bool not_at_start) {
  if (budget <= 0) return 0;
  int min = 100;
  int choice_count = alternatives_->length();
  budget = (budget - 1) / choice_count;
  for (int i = 0; i < choice_count; i++) {
    RegExpNode* node = alternatives_->at(i).node();
    if (node == ignore_this_node) continue;
    int node_eats_at_least =
        node->EatsAtLeast(still_to_find, budget, not_at_start);
    if (node_eats_at_least < min) min = node_eats_at_least;
    if (min == 0) return 0;
  }
  return min;
}


int LoopChoiceNode::EatsAtLeast(int still_to_find,
                                int budget,
                                bool not_at_start) {
  return EatsAtLeastHelper(still_to_find,
                           budget - 1,
                           loop_node_,
                           not_at_start);
}


int ChoiceNode::EatsAtLeast(int still_to_find,
                            int budget,
                            bool not_at_start) {
  return EatsAtLeastHelper(still_to_find, budget, nullptr, not_at_start);
}


// Takes the left-most 1-bit and smears it out, setting all bits to its right.
static inline uint32_t SmearBitsRight(uint32_t v) {
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v;
}


bool QuickCheckDetails::Rationalize(bool asc) {
  bool found_useful_op = false;
  uint32_t char_mask;
  if (asc) {
    char_mask = String::kMaxOneByteCharCode;
  } else {
    char_mask = String::kMaxUtf16CodeUnit;
  }
  mask_ = 0;
  value_ = 0;
  int char_shift = 0;
  for (int i = 0; i < characters_; i++) {
    Position* pos = &positions_[i];
    if ((pos->mask & String::kMaxOneByteCharCode) != 0) {
      found_useful_op = true;
    }
    mask_ |= (pos->mask & char_mask) << char_shift;
    value_ |= (pos->value & char_mask) << char_shift;
    char_shift += asc ? 8 : 16;
  }
  return found_useful_op;
}


bool RegExpNode::EmitQuickCheck(RegExpCompiler* compiler,
                                Trace* bounds_check_trace,
                                Trace* trace,
                                bool preload_has_checked_bounds,
                                Label* on_possible_success,
                                QuickCheckDetails* details,
                                bool fall_through_on_failure) {
  if (details->characters() == 0) return false;
  GetQuickCheckDetails(
      details, compiler, 0, trace->at_start() == Trace::FALSE_VALUE);
  if (details->cannot_match()) return false;
  if (!details->Rationalize(compiler->one_byte())) return false;
  DCHECK(details->characters() == 1 ||
         compiler->macro_assembler()->CanReadUnaligned());
  uint32_t mask = details->mask();
  uint32_t value = details->value();

  RegExpMacroAssembler* assembler = compiler->macro_assembler();

  if (trace->characters_preloaded() != details->characters()) {
    DCHECK(trace->cp_offset() == bounds_check_trace->cp_offset());
    // We are attempting to preload the minimum number of characters
    // any choice would eat, so if the bounds check fails, then none of the
    // choices can succeed, so we can just immediately backtrack, rather
    // than go to the next choice.
    assembler->LoadCurrentCharacter(trace->cp_offset(),
                                    bounds_check_trace->backtrack(),
                                    !preload_has_checked_bounds,
                                    details->characters());
  }


  bool need_mask = true;

  if (details->characters() == 1) {
    // If number of characters preloaded is 1 then we used a byte or 16 bit
    // load so the value is already masked down.
    uint32_t char_mask;
    if (compiler->one_byte()) {
      char_mask = String::kMaxOneByteCharCode;
    } else {
      char_mask = String::kMaxUtf16CodeUnit;
    }
    if ((mask & char_mask) == char_mask) need_mask = false;
    mask &= char_mask;
  } else {
    // For 2-character preloads in one-byte mode or 1-character preloads in
    // two-byte mode we also use a 16 bit load with zero extend.
    static const uint32_t kTwoByteMask = 0xffff;
    static const uint32_t kFourByteMask = 0xffffffff;
    if (details->characters() == 2 && compiler->one_byte()) {
      if ((mask & kTwoByteMask) == kTwoByteMask) need_mask = false;
    } else if (details->characters() == 1 && !compiler->one_byte()) {
      if ((mask & kTwoByteMask) == kTwoByteMask) need_mask = false;
    } else {
      if (mask == kFourByteMask) need_mask = false;
    }
  }

  if (fall_through_on_failure) {
    if (need_mask) {
      assembler->CheckCharacterAfterAnd(value, mask, on_possible_success);
    } else {
      assembler->CheckCharacter(value, on_possible_success);
    }
  } else {
    if (need_mask) {
      assembler->CheckNotCharacterAfterAnd(value, mask, trace->backtrack());
    } else {
      assembler->CheckNotCharacter(value, trace->backtrack());
    }
  }
  return true;
}


// Here is the meat of GetQuickCheckDetails (see also the comment on the
// super-class in the .h file).
//
// We iterate along the text object, building up for each character a
// mask and value that can be used to test for a quick failure to match.
// The masks and values for the positions will be combined into a single
// machine word for the current character width in order to be used in
// generating a quick check.
void TextNode::GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start) {
  // Do not collect any quick check details if the text node reads backward,
  // since it reads in the opposite direction than we use for quick checks.
  if (read_backward()) return;
  Isolate* isolate = compiler->macro_assembler()->isolate();
  DCHECK(characters_filled_in < details->characters());
  int characters = details->characters();
  int char_mask;
  if (compiler->one_byte()) {
    char_mask = String::kMaxOneByteCharCode;
  } else {
    char_mask = String::kMaxUtf16CodeUnit;
  }
  for (int k = 0; k < elements()->length(); k++) {
    TextElement elm = elements()->at(k);
    if (elm.text_type() == TextElement::ATOM) {
      Vector<const uc16> quarks = elm.atom()->data();
      for (int i = 0; i < characters && i < quarks.length(); i++) {
        QuickCheckDetails::Position* pos =
            details->positions(characters_filled_in);
        uc16 c = quarks[i];
        if (elm.atom()->ignore_case()) {
          unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
          int length = GetCaseIndependentLetters(isolate, c,
                                                 compiler->one_byte(), chars);
          if (length == 0) {
            // This can happen because all case variants are non-Latin1, but we
            // know the input is Latin1.
            details->set_cannot_match();
            pos->determines_perfectly = false;
            return;
          }
          if (length == 1) {
            // This letter has no case equivalents, so it's nice and simple
            // and the mask-compare will determine definitely whether we have
            // a match at this character position.
            pos->mask = char_mask;
            pos->value = c;
            pos->determines_perfectly = true;
          } else {
            uint32_t common_bits = char_mask;
            uint32_t bits = chars[0];
            for (int j = 1; j < length; j++) {
              uint32_t differing_bits = ((chars[j] & common_bits) ^ bits);
              common_bits ^= differing_bits;
              bits &= common_bits;
            }
            // If length is 2 and common bits has only one zero in it then
            // our mask and compare instruction will determine definitely
            // whether we have a match at this character position.  Otherwise
            // it can only be an approximate check.
            uint32_t one_zero = (common_bits | ~char_mask);
            if (length == 2 && ((~one_zero) & ((~one_zero) - 1)) == 0) {
              pos->determines_perfectly = true;
            }
            pos->mask = common_bits;
            pos->value = bits;
          }
        } else {
          // Don't ignore case.  Nice simple case where the mask-compare will
          // determine definitely whether we have a match at this character
          // position.
          if (c > char_mask) {
            details->set_cannot_match();
            pos->determines_perfectly = false;
            return;
          }
          pos->mask = char_mask;
          pos->value = c;
          pos->determines_perfectly = true;
        }
        characters_filled_in++;
        DCHECK(characters_filled_in <= details->characters());
        if (characters_filled_in == details->characters()) {
          return;
        }
      }
    } else {
      QuickCheckDetails::Position* pos =
          details->positions(characters_filled_in);
      RegExpCharacterClass* tree = elm.char_class();
      ZoneList<CharacterRange>* ranges = tree->ranges(zone());
      if (tree->is_negated()) {
        // A quick check uses multi-character mask and compare.  There is no
        // useful way to incorporate a negative char class into this scheme
        // so we just conservatively create a mask and value that will always
        // succeed.
        pos->mask = 0;
        pos->value = 0;
      } else {
        int first_range = 0;
        while (ranges->at(first_range).from() > char_mask) {
          first_range++;
          if (first_range == ranges->length()) {
            details->set_cannot_match();
            pos->determines_perfectly = false;
            return;
          }
        }
        CharacterRange range = ranges->at(first_range);
        uc16 from = range.from();
        uc16 to = range.to();
        if (to > char_mask) {
          to = char_mask;
        }
        uint32_t differing_bits = (from ^ to);
        // A mask and compare is only perfect if the differing bits form a
        // number like 00011111 with one single block of trailing 1s.
        if ((differing_bits & (differing_bits + 1)) == 0 &&
             from + differing_bits == to) {
          pos->determines_perfectly = true;
        }
        uint32_t common_bits = ~SmearBitsRight(differing_bits);
        uint32_t bits = (from & common_bits);
        for (int i = first_range + 1; i < ranges->length(); i++) {
          CharacterRange range = ranges->at(i);
          uc16 from = range.from();
          uc16 to = range.to();
          if (from > char_mask) continue;
          if (to > char_mask) to = char_mask;
          // Here we are combining more ranges into the mask and compare
          // value.  With each new range the mask becomes more sparse and
          // so the chances of a false positive rise.  A character class
          // with multiple ranges is assumed never to be equivalent to a
          // mask and compare operation.
          pos->determines_perfectly = false;
          uint32_t new_common_bits = (from ^ to);
          new_common_bits = ~SmearBitsRight(new_common_bits);
          common_bits &= new_common_bits;
          bits &= new_common_bits;
          uint32_t differing_bits = (from & common_bits) ^ bits;
          common_bits ^= differing_bits;
          bits &= common_bits;
        }
        pos->mask = common_bits;
        pos->value = bits;
      }
      characters_filled_in++;
      DCHECK(characters_filled_in <= details->characters());
      if (characters_filled_in == details->characters()) {
        return;
      }
    }
  }
  DCHECK(characters_filled_in != details->characters());
  if (!details->cannot_match()) {
    on_success()-> GetQuickCheckDetails(details,
                                        compiler,
                                        characters_filled_in,
                                        true);
  }
}


void QuickCheckDetails::Clear() {
  for (int i = 0; i < characters_; i++) {
    positions_[i].mask = 0;
    positions_[i].value = 0;
    positions_[i].determines_perfectly = false;
  }
  characters_ = 0;
}


void QuickCheckDetails::Advance(int by, bool one_byte) {
  if (by >= characters_ || by < 0) {
    DCHECK_IMPLIES(by < 0, characters_ == 0);
    Clear();
    return;
  }
  DCHECK_LE(characters_ - by, 4);
  DCHECK_LE(characters_, 4);
  for (int i = 0; i < characters_ - by; i++) {
    positions_[i] = positions_[by + i];
  }
  for (int i = characters_ - by; i < characters_; i++) {
    positions_[i].mask = 0;
    positions_[i].value = 0;
    positions_[i].determines_perfectly = false;
  }
  characters_ -= by;
  // We could change mask_ and value_ here but we would never advance unless
  // they had already been used in a check and they won't be used again because
  // it would gain us nothing.  So there's no point.
}


void QuickCheckDetails::Merge(QuickCheckDetails* other, int from_index) {
  DCHECK(characters_ == other->characters_);
  if (other->cannot_match_) {
    return;
  }
  if (cannot_match_) {
    *this = *other;
    return;
  }
  for (int i = from_index; i < characters_; i++) {
    QuickCheckDetails::Position* pos = positions(i);
    QuickCheckDetails::Position* other_pos = other->positions(i);
    if (pos->mask != other_pos->mask ||
        pos->value != other_pos->value ||
        !other_pos->determines_perfectly) {
      // Our mask-compare operation will be approximate unless we have the
      // exact same operation on both sides of the alternation.
      pos->determines_perfectly = false;
    }
    pos->mask &= other_pos->mask;
    pos->value &= pos->mask;
    other_pos->value &= pos->mask;
    uc16 differing_bits = (pos->value ^ other_pos->value);
    pos->mask &= ~differing_bits;
    pos->value &= pos->mask;
  }
}


class VisitMarker {
 public:
  explicit VisitMarker(NodeInfo* info) : info_(info) {
    DCHECK(!info->visited);
    info->visited = true;
  }
  ~VisitMarker() {
    info_->visited = false;
  }
 private:
  NodeInfo* info_;
};

RegExpNode* SeqRegExpNode::FilterOneByte(int depth) {
  if (info()->replacement_calculated) return replacement();
  if (depth < 0) return this;
  DCHECK(!info()->visited);
  VisitMarker marker(info());
  return FilterSuccessor(depth - 1);
}

RegExpNode* SeqRegExpNode::FilterSuccessor(int depth) {
  RegExpNode* next = on_success_->FilterOneByte(depth - 1);
  if (next == nullptr) return set_replacement(nullptr);
  on_success_ = next;
  return set_replacement(this);
}


// We need to check for the following characters: 0x39c 0x3bc 0x178.
static inline bool RangeContainsLatin1Equivalents(CharacterRange range) {
  // TODO(dcarney): this could be a lot more efficient.
  return range.Contains(0x39c) ||
      range.Contains(0x3bc) || range.Contains(0x178);
}


static bool RangesContainLatin1Equivalents(ZoneList<CharacterRange>* ranges) {
  for (int i = 0; i < ranges->length(); i++) {
    // TODO(dcarney): this could be a lot more efficient.
    if (RangeContainsLatin1Equivalents(ranges->at(i))) return true;
  }
  return false;
}

RegExpNode* TextNode::FilterOneByte(int depth) {
  if (info()->replacement_calculated) return replacement();
  if (depth < 0) return this;
  DCHECK(!info()->visited);
  VisitMarker marker(info());
  int element_count = elements()->length();
  for (int i = 0; i < element_count; i++) {
    TextElement elm = elements()->at(i);
    if (elm.text_type() == TextElement::ATOM) {
      Vector<const uc16> quarks = elm.atom()->data();
      for (int j = 0; j < quarks.length(); j++) {
        uint16_t c = quarks[j];
        if (c <= String::kMaxOneByteCharCode) continue;
        if (!IgnoreCase(elm.atom()->flags())) return set_replacement(nullptr);
        // Here, we need to check for characters whose upper and lower cases
        // are outside the Latin-1 range.
        uint16_t converted = unibrow::Latin1::ConvertNonLatin1ToLatin1(c);
        // Character is outside Latin-1 completely
        if (converted == 0) return set_replacement(nullptr);
        // Convert quark to Latin-1 in place.
        uint16_t* copy = const_cast<uint16_t*>(quarks.start());
        copy[j] = converted;
      }
    } else {
      DCHECK(elm.text_type() == TextElement::CHAR_CLASS);
      RegExpCharacterClass* cc = elm.char_class();
      ZoneList<CharacterRange>* ranges = cc->ranges(zone());
      CharacterRange::Canonicalize(ranges);
      // Now they are in order so we only need to look at the first.
      int range_count = ranges->length();
      if (cc->is_negated()) {
        if (range_count != 0 &&
            ranges->at(0).from() == 0 &&
            ranges->at(0).to() >= String::kMaxOneByteCharCode) {
          // This will be handled in a later filter.
          if (IgnoreCase(cc->flags()) && RangesContainLatin1Equivalents(ranges))
            continue;
          return set_replacement(nullptr);
        }
      } else {
        if (range_count == 0 ||
            ranges->at(0).from() > String::kMaxOneByteCharCode) {
          // This will be handled in a later filter.
          if (IgnoreCase(cc->flags()) && RangesContainLatin1Equivalents(ranges))
            continue;
          return set_replacement(nullptr);
        }
      }
    }
  }
  return FilterSuccessor(depth - 1);
}

RegExpNode* LoopChoiceNode::FilterOneByte(int depth) {
  if (info()->replacement_calculated) return replacement();
  if (depth < 0) return this;
  if (info()->visited) return this;
  {
    VisitMarker marker(info());

    RegExpNode* continue_replacement = continue_node_->FilterOneByte(depth - 1);
    // If we can't continue after the loop then there is no sense in doing the
    // loop.
    if (continue_replacement == nullptr) return set_replacement(nullptr);
  }

  return ChoiceNode::FilterOneByte(depth - 1);
}

RegExpNode* ChoiceNode::FilterOneByte(int depth) {
  if (info()->replacement_calculated) return replacement();
  if (depth < 0) return this;
  if (info()->visited) return this;
  VisitMarker marker(info());
  int choice_count = alternatives_->length();

  for (int i = 0; i < choice_count; i++) {
    GuardedAlternative alternative = alternatives_->at(i);
    if (alternative.guards() != nullptr &&
        alternative.guards()->length() != 0) {
      set_replacement(this);
      return this;
    }
  }

  int surviving = 0;
  RegExpNode* survivor = nullptr;
  for (int i = 0; i < choice_count; i++) {
    GuardedAlternative alternative = alternatives_->at(i);
    RegExpNode* replacement = alternative.node()->FilterOneByte(depth - 1);
    DCHECK(replacement != this);  // No missing EMPTY_MATCH_CHECK.
    if (replacement != nullptr) {
      alternatives_->at(i).set_node(replacement);
      surviving++;
      survivor = replacement;
    }
  }
  if (surviving < 2) return set_replacement(survivor);

  set_replacement(this);
  if (surviving == choice_count) {
    return this;
  }
  // Only some of the nodes survived the filtering.  We need to rebuild the
  // alternatives list.
  ZoneList<GuardedAlternative>* new_alternatives =
      new(zone()) ZoneList<GuardedAlternative>(surviving, zone());
  for (int i = 0; i < choice_count; i++) {
    RegExpNode* replacement =
        alternatives_->at(i).node()->FilterOneByte(depth - 1);
    if (replacement != nullptr) {
      alternatives_->at(i).set_node(replacement);
      new_alternatives->Add(alternatives_->at(i), zone());
    }
  }
  alternatives_ = new_alternatives;
  return this;
}

RegExpNode* NegativeLookaroundChoiceNode::FilterOneByte(int depth) {
  if (info()->replacement_calculated) return replacement();
  if (depth < 0) return this;
  if (info()->visited) return this;
  VisitMarker marker(info());
  // Alternative 0 is the negative lookahead, alternative 1 is what comes
  // afterwards.
  RegExpNode* node = alternatives_->at(1).node();
  RegExpNode* replacement = node->FilterOneByte(depth - 1);
  if (replacement == nullptr) return set_replacement(nullptr);
  alternatives_->at(1).set_node(replacement);

  RegExpNode* neg_node = alternatives_->at(0).node();
  RegExpNode* neg_replacement = neg_node->FilterOneByte(depth - 1);
  // If the negative lookahead is always going to fail then
  // we don't need to check it.
  if (neg_replacement == nullptr) return set_replacement(replacement);
  alternatives_->at(0).set_node(neg_replacement);
  return set_replacement(this);
}


void LoopChoiceNode::GetQuickCheckDetails(QuickCheckDetails* details,
                                          RegExpCompiler* compiler,
                                          int characters_filled_in,
                                          bool not_at_start) {
  if (body_can_be_zero_length_ || info()->visited) return;
  VisitMarker marker(info());
  return ChoiceNode::GetQuickCheckDetails(details,
                                          compiler,
                                          characters_filled_in,
                                          not_at_start);
}


void LoopChoiceNode::FillInBMInfo(Isolate* isolate, int offset, int budget,
                                  BoyerMooreLookahead* bm, bool not_at_start) {
  if (body_can_be_zero_length_ || budget <= 0) {
    bm->SetRest(offset);
    SaveBMInfo(bm, not_at_start, offset);
    return;
  }
  ChoiceNode::FillInBMInfo(isolate, offset, budget - 1, bm, not_at_start);
  SaveBMInfo(bm, not_at_start, offset);
}


void ChoiceNode::GetQuickCheckDetails(QuickCheckDetails* details,
                                      RegExpCompiler* compiler,
                                      int characters_filled_in,
                                      bool not_at_start) {
  not_at_start = (not_at_start || not_at_start_);
  int choice_count = alternatives_->length();
  DCHECK_LT(0, choice_count);
  alternatives_->at(0).node()->GetQuickCheckDetails(details,
                                                    compiler,
                                                    characters_filled_in,
                                                    not_at_start);
  for (int i = 1; i < choice_count; i++) {
    QuickCheckDetails new_details(details->characters());
    RegExpNode* node = alternatives_->at(i).node();
    node->GetQuickCheckDetails(&new_details, compiler,
                               characters_filled_in,
                               not_at_start);
    // Here we merge the quick match details of the two branches.
    details->Merge(&new_details, characters_filled_in);
  }
}


// Check for [0-9A-Z_a-z].
static void EmitWordCheck(RegExpMacroAssembler* assembler,
                          Label* word,
                          Label* non_word,
                          bool fall_through_on_word) {
  if (assembler->CheckSpecialCharacterClass(
          fall_through_on_word ? 'w' : 'W',
          fall_through_on_word ? non_word : word)) {
    // Optimized implementation available.
    return;
  }
  assembler->CheckCharacterGT('z', non_word);
  assembler->CheckCharacterLT('0', non_word);
  assembler->CheckCharacterGT('a' - 1, word);
  assembler->CheckCharacterLT('9' + 1, word);
  assembler->CheckCharacterLT('A', non_word);
  assembler->CheckCharacterLT('Z' + 1, word);
  if (fall_through_on_word) {
    assembler->CheckNotCharacter('_', non_word);
  } else {
    assembler->CheckCharacter('_', word);
  }
}


// Emit the code to check for a ^ in multiline mode (1-character lookbehind
// that matches newline or the start of input).
static void EmitHat(RegExpCompiler* compiler,
                    RegExpNode* on_success,
                    Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  // We will be loading the previous character into the current character
  // register.
  Trace new_trace(*trace);
  new_trace.InvalidateCurrentCharacter();

  Label ok;
  if (new_trace.cp_offset() == 0) {
    // The start of input counts as a newline in this context, so skip to
    // ok if we are at the start.
    assembler->CheckAtStart(&ok);
  }
  // We already checked that we are not at the start of input so it must be
  // OK to load the previous character.
  assembler->LoadCurrentCharacter(new_trace.cp_offset() -1,
                                  new_trace.backtrack(),
                                  false);
  if (!assembler->CheckSpecialCharacterClass('n',
                                             new_trace.backtrack())) {
    // Newline means \n, \r, 0x2028 or 0x2029.
    if (!compiler->one_byte()) {
      assembler->CheckCharacterAfterAnd(0x2028, 0xfffe, &ok);
    }
    assembler->CheckCharacter('\n', &ok);
    assembler->CheckNotCharacter('\r', new_trace.backtrack());
  }
  assembler->Bind(&ok);
  on_success->Emit(compiler, &new_trace);
}

// More makes code generation slower, less makes V8 benchmark score lower.
const int kMaxLookaheadForBoyerMoore = 8;

// Emit the code to handle \b and \B (word-boundary or non-word-boundary).
void AssertionNode::EmitBoundaryCheck(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  Isolate* isolate = assembler->isolate();
  Trace::TriBool next_is_word_character = Trace::UNKNOWN;
  bool not_at_start = (trace->at_start() == Trace::FALSE_VALUE);
  BoyerMooreLookahead* lookahead = bm_info(not_at_start);
  if (lookahead == nullptr) {
    int eats_at_least =
        Min(kMaxLookaheadForBoyerMoore, EatsAtLeast(kMaxLookaheadForBoyerMoore,
                                                    kRecursionBudget,
                                                    not_at_start));
    if (eats_at_least >= 1) {
      BoyerMooreLookahead* bm =
          new(zone()) BoyerMooreLookahead(eats_at_least, compiler, zone());
      FillInBMInfo(isolate, 0, kRecursionBudget, bm, not_at_start);
      if (bm->at(0)->is_non_word())
        next_is_word_character = Trace::FALSE_VALUE;
      if (bm->at(0)->is_word()) next_is_word_character = Trace::TRUE_VALUE;
    }
  } else {
    if (lookahead->at(0)->is_non_word())
      next_is_word_character = Trace::FALSE_VALUE;
    if (lookahead->at(0)->is_word())
      next_is_word_character = Trace::TRUE_VALUE;
  }
  bool at_boundary = (assertion_type_ == AssertionNode::AT_BOUNDARY);
  if (next_is_word_character == Trace::UNKNOWN) {
    Label before_non_word;
    Label before_word;
    if (trace->characters_preloaded() != 1) {
      assembler->LoadCurrentCharacter(trace->cp_offset(), &before_non_word);
    }
    // Fall through on non-word.
    EmitWordCheck(assembler, &before_word, &before_non_word, false);
    // Next character is not a word character.
    assembler->Bind(&before_non_word);
    Label ok;
    BacktrackIfPrevious(compiler, trace, at_boundary ? kIsNonWord : kIsWord);
    assembler->GoTo(&ok);

    assembler->Bind(&before_word);
    BacktrackIfPrevious(compiler, trace, at_boundary ? kIsWord : kIsNonWord);
    assembler->Bind(&ok);
  } else if (next_is_word_character == Trace::TRUE_VALUE) {
    BacktrackIfPrevious(compiler, trace, at_boundary ? kIsWord : kIsNonWord);
  } else {
    DCHECK(next_is_word_character == Trace::FALSE_VALUE);
    BacktrackIfPrevious(compiler, trace, at_boundary ? kIsNonWord : kIsWord);
  }
}


void AssertionNode::BacktrackIfPrevious(
    RegExpCompiler* compiler,
    Trace* trace,
    AssertionNode::IfPrevious backtrack_if_previous) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  Trace new_trace(*trace);
  new_trace.InvalidateCurrentCharacter();

  Label fall_through, dummy;

  Label* non_word = backtrack_if_previous == kIsNonWord ?
                    new_trace.backtrack() :
                    &fall_through;
  Label* word = backtrack_if_previous == kIsNonWord ?
                &fall_through :
                new_trace.backtrack();

  if (new_trace.cp_offset() == 0) {
    // The start of input counts as a non-word character, so the question is
    // decided if we are at the start.
    assembler->CheckAtStart(non_word);
  }
  // We already checked that we are not at the start of input so it must be
  // OK to load the previous character.
  assembler->LoadCurrentCharacter(new_trace.cp_offset() - 1, &dummy, false);
  EmitWordCheck(assembler, word, non_word, backtrack_if_previous == kIsNonWord);

  assembler->Bind(&fall_through);
  on_success()->Emit(compiler, &new_trace);
}


void AssertionNode::GetQuickCheckDetails(QuickCheckDetails* details,
                                         RegExpCompiler* compiler,
                                         int filled_in,
                                         bool not_at_start) {
  if (assertion_type_ == AT_START && not_at_start) {
    details->set_cannot_match();
    return;
  }
  return on_success()->GetQuickCheckDetails(details,
                                            compiler,
                                            filled_in,
                                            not_at_start);
}


void AssertionNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  switch (assertion_type_) {
    case AT_END: {
      Label ok;
      assembler->CheckPosition(trace->cp_offset(), &ok);
      assembler->GoTo(trace->backtrack());
      assembler->Bind(&ok);
      break;
    }
    case AT_START: {
      if (trace->at_start() == Trace::FALSE_VALUE) {
        assembler->GoTo(trace->backtrack());
        return;
      }
      if (trace->at_start() == Trace::UNKNOWN) {
        assembler->CheckNotAtStart(trace->cp_offset(), trace->backtrack());
        Trace at_start_trace = *trace;
        at_start_trace.set_at_start(Trace::TRUE_VALUE);
        on_success()->Emit(compiler, &at_start_trace);
        return;
      }
    }
    break;
    case AFTER_NEWLINE:
      EmitHat(compiler, on_success(), trace);
      return;
    case AT_BOUNDARY:
    case AT_NON_BOUNDARY: {
      EmitBoundaryCheck(compiler, trace);
      return;
    }
  }
  on_success()->Emit(compiler, trace);
}


static bool DeterminedAlready(QuickCheckDetails* quick_check, int offset) {
  if (quick_check == nullptr) return false;
  if (offset >= quick_check->characters()) return false;
  return quick_check->positions(offset)->determines_perfectly;
}


static void UpdateBoundsCheck(int index, int* checked_up_to) {
  if (index > *checked_up_to) {
    *checked_up_to = index;
  }
}


// We call this repeatedly to generate code for each pass over the text node.
// The passes are in increasing order of difficulty because we hope one
// of the first passes will fail in which case we are saved the work of the
// later passes.  for example for the case independent regexp /%[asdfghjkl]a/
// we will check the '%' in the first pass, the case independent 'a' in the
// second pass and the character class in the last pass.
//
// The passes are done from right to left, so for example to test for /bar/
// we will first test for an 'r' with offset 2, then an 'a' with offset 1
// and then a 'b' with offset 0.  This means we can avoid the end-of-input
// bounds check most of the time.  In the example we only need to check for
// end-of-input when loading the putative 'r'.
//
// A slight complication involves the fact that the first character may already
// be fetched into a register by the previous node.  In this case we want to
// do the test for that character first.  We do this in separate passes.  The
// 'preloaded' argument indicates that we are doing such a 'pass'.  If such a
// pass has been performed then subsequent passes will have true in
// first_element_checked to indicate that that character does not need to be
// checked again.
//
// In addition to all this we are passed a Trace, which can
// contain an AlternativeGeneration object.  In this AlternativeGeneration
// object we can see details of any quick check that was already passed in
// order to get to the code we are now generating.  The quick check can involve
// loading characters, which means we do not need to recheck the bounds
// up to the limit the quick check already checked.  In addition the quick
// check can have involved a mask and compare operation which may simplify
// or obviate the need for further checks at some character positions.
void TextNode::TextEmitPass(RegExpCompiler* compiler,
                            TextEmitPassType pass,
                            bool preloaded,
                            Trace* trace,
                            bool first_element_checked,
                            int* checked_up_to) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  Isolate* isolate = assembler->isolate();
  bool one_byte = compiler->one_byte();
  Label* backtrack = trace->backtrack();
  QuickCheckDetails* quick_check = trace->quick_check_performed();
  int element_count = elements()->length();
  int backward_offset = read_backward() ? -Length() : 0;
  for (int i = preloaded ? 0 : element_count - 1; i >= 0; i--) {
    TextElement elm = elements()->at(i);
    int cp_offset = trace->cp_offset() + elm.cp_offset() + backward_offset;
    if (elm.text_type() == TextElement::ATOM) {
      if (SkipPass(pass, elm.atom()->ignore_case())) continue;
      Vector<const uc16> quarks = elm.atom()->data();
      for (int j = preloaded ? 0 : quarks.length() - 1; j >= 0; j--) {
        if (first_element_checked && i == 0 && j == 0) continue;
        if (DeterminedAlready(quick_check, elm.cp_offset() + j)) continue;
        EmitCharacterFunction* emit_function = nullptr;
        switch (pass) {
          case NON_LATIN1_MATCH:
            DCHECK(one_byte);
            if (quarks[j] > String::kMaxOneByteCharCode) {
              assembler->GoTo(backtrack);
              return;
            }
            break;
          case NON_LETTER_CHARACTER_MATCH:
            emit_function = &EmitAtomNonLetter;
            break;
          case SIMPLE_CHARACTER_MATCH:
            emit_function = &EmitSimpleCharacter;
            break;
          case CASE_CHARACTER_MATCH:
            emit_function = &EmitAtomLetter;
            break;
          default:
            break;
        }
        if (emit_function != nullptr) {
          bool bounds_check = *checked_up_to < cp_offset + j || read_backward();
          bool bound_checked =
              emit_function(isolate, compiler, quarks[j], backtrack,
                            cp_offset + j, bounds_check, preloaded);
          if (bound_checked) UpdateBoundsCheck(cp_offset + j, checked_up_to);
        }
      }
    } else {
      DCHECK_EQ(TextElement::CHAR_CLASS, elm.text_type());
      if (pass == CHARACTER_CLASS_MATCH) {
        if (first_element_checked && i == 0) continue;
        if (DeterminedAlready(quick_check, elm.cp_offset())) continue;
        RegExpCharacterClass* cc = elm.char_class();
        bool bounds_check = *checked_up_to < cp_offset || read_backward();
        EmitCharClass(assembler, cc, one_byte, backtrack, cp_offset,
                      bounds_check, preloaded, zone());
        UpdateBoundsCheck(cp_offset, checked_up_to);
      }
    }
  }
}


int TextNode::Length() {
  TextElement elm = elements()->last();
  DCHECK_LE(0, elm.cp_offset());
  return elm.cp_offset() + elm.length();
}

bool TextNode::SkipPass(TextEmitPassType pass, bool ignore_case) {
  if (ignore_case) {
    return pass == SIMPLE_CHARACTER_MATCH;
  } else {
    return pass == NON_LETTER_CHARACTER_MATCH || pass == CASE_CHARACTER_MATCH;
  }
}

TextNode* TextNode::CreateForCharacterRanges(Zone* zone,
                                             ZoneList<CharacterRange>* ranges,
                                             bool read_backward,
                                             RegExpNode* on_success,
                                             JSRegExp::Flags flags) {
  DCHECK_NOT_NULL(ranges);
  ZoneList<TextElement>* elms = new (zone) ZoneList<TextElement>(1, zone);
  elms->Add(
      TextElement::CharClass(new (zone) RegExpCharacterClass(ranges, flags)),
      zone);
  return new (zone) TextNode(elms, read_backward, on_success);
}

TextNode* TextNode::CreateForSurrogatePair(Zone* zone, CharacterRange lead,
                                           CharacterRange trail,
                                           bool read_backward,
                                           RegExpNode* on_success,
                                           JSRegExp::Flags flags) {
  ZoneList<CharacterRange>* lead_ranges = CharacterRange::List(zone, lead);
  ZoneList<CharacterRange>* trail_ranges = CharacterRange::List(zone, trail);
  ZoneList<TextElement>* elms = new (zone) ZoneList<TextElement>(2, zone);
  elms->Add(TextElement::CharClass(
                new (zone) RegExpCharacterClass(lead_ranges, flags)),
            zone);
  elms->Add(TextElement::CharClass(
                new (zone) RegExpCharacterClass(trail_ranges, flags)),
            zone);
  return new (zone) TextNode(elms, read_backward, on_success);
}


// This generates the code to match a text node.  A text node can contain
// straight character sequences (possibly to be matched in a case-independent
// way) and character classes.  For efficiency we do not do this in a single
// pass from left to right.  Instead we pass over the text node several times,
// emitting code for some character positions every time.  See the comment on
// TextEmitPass for details.
void TextNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  LimitResult limit_result = LimitVersions(compiler, trace);
  if (limit_result == DONE) return;
  DCHECK(limit_result == CONTINUE);

  if (trace->cp_offset() + Length() > RegExpMacroAssembler::kMaxCPOffset) {
    compiler->SetRegExpTooBig();
    return;
  }

  if (compiler->one_byte()) {
    int dummy = 0;
    TextEmitPass(compiler, NON_LATIN1_MATCH, false, trace, false, &dummy);
  }

  bool first_elt_done = false;
  int bound_checked_to = trace->cp_offset() - 1;
  bound_checked_to += trace->bound_checked_up_to();

  // If a character is preloaded into the current character register then
  // check that now.
  if (trace->characters_preloaded() == 1) {
    for (int pass = kFirstRealPass; pass <= kLastPass; pass++) {
      TextEmitPass(compiler, static_cast<TextEmitPassType>(pass), true, trace,
                   false, &bound_checked_to);
    }
    first_elt_done = true;
  }

  for (int pass = kFirstRealPass; pass <= kLastPass; pass++) {
    TextEmitPass(compiler, static_cast<TextEmitPassType>(pass), false, trace,
                 first_elt_done, &bound_checked_to);
  }

  Trace successor_trace(*trace);
  // If we advance backward, we may end up at the start.
  successor_trace.AdvanceCurrentPositionInTrace(
      read_backward() ? -Length() : Length(), compiler);
  successor_trace.set_at_start(read_backward() ? Trace::UNKNOWN
                                               : Trace::FALSE_VALUE);
  RecursionCheck rc(compiler);
  on_success()->Emit(compiler, &successor_trace);
}


void Trace::InvalidateCurrentCharacter() {
  characters_preloaded_ = 0;
}


void Trace::AdvanceCurrentPositionInTrace(int by, RegExpCompiler* compiler) {
  // We don't have an instruction for shifting the current character register
  // down or for using a shifted value for anything so lets just forget that
  // we preloaded any characters into it.
  characters_preloaded_ = 0;
  // Adjust the offsets of the quick check performed information.  This
  // information is used to find out what we already determined about the
  // characters by means of mask and compare.
  quick_check_performed_.Advance(by, compiler->one_byte());
  cp_offset_ += by;
  if (cp_offset_ > RegExpMacroAssembler::kMaxCPOffset) {
    compiler->SetRegExpTooBig();
    cp_offset_ = 0;
  }
  bound_checked_up_to_ = Max(0, bound_checked_up_to_ - by);
}


void TextNode::MakeCaseIndependent(Isolate* isolate, bool is_one_byte) {
  int element_count = elements()->length();
  for (int i = 0; i < element_count; i++) {
    TextElement elm = elements()->at(i);
    if (elm.text_type() == TextElement::CHAR_CLASS) {
      RegExpCharacterClass* cc = elm.char_class();
#ifdef V8_INTL_SUPPORT
      bool case_equivalents_already_added =
          NeedsUnicodeCaseEquivalents(cc->flags());
#else
      bool case_equivalents_already_added = false;
#endif
      if (IgnoreCase(cc->flags()) && !case_equivalents_already_added) {
        // None of the standard character classes is different in the case
        // independent case and it slows us down if we don't know that.
        if (cc->is_standard(zone())) continue;
        ZoneList<CharacterRange>* ranges = cc->ranges(zone());
        CharacterRange::AddCaseEquivalents(isolate, zone(), ranges,
                                           is_one_byte);
      }
    }
  }
}


int TextNode::GreedyLoopTextLength() { return Length(); }


RegExpNode* TextNode::GetSuccessorOfOmnivorousTextNode(
    RegExpCompiler* compiler) {
  if (read_backward()) return nullptr;
  if (elements()->length() != 1) return nullptr;
  TextElement elm = elements()->at(0);
  if (elm.text_type() != TextElement::CHAR_CLASS) return nullptr;
  RegExpCharacterClass* node = elm.char_class();
  ZoneList<CharacterRange>* ranges = node->ranges(zone());
  CharacterRange::Canonicalize(ranges);
  if (node->is_negated()) {
    return ranges->length() == 0 ? on_success() : nullptr;
  }
  if (ranges->length() != 1) return nullptr;
  uint32_t max_char;
  if (compiler->one_byte()) {
    max_char = String::kMaxOneByteCharCode;
  } else {
    max_char = String::kMaxUtf16CodeUnit;
  }
  return ranges->at(0).IsEverything(max_char) ? on_success() : nullptr;
}


// Finds the fixed match length of a sequence of nodes that goes from
// this alternative and back to this choice node.  If there are variable
// length nodes or other complications in the way then return a sentinel
// value indicating that a greedy loop cannot be constructed.
int ChoiceNode::GreedyLoopTextLengthForAlternative(
    GuardedAlternative* alternative) {
  int length = 0;
  RegExpNode* node = alternative->node();
  // Later we will generate code for all these text nodes using recursion
  // so we have to limit the max number.
  int recursion_depth = 0;
  while (node != this) {
    if (recursion_depth++ > RegExpCompiler::kMaxRecursion) {
      return kNodeIsTooComplexForGreedyLoops;
    }
    int node_length = node->GreedyLoopTextLength();
    if (node_length == kNodeIsTooComplexForGreedyLoops) {
      return kNodeIsTooComplexForGreedyLoops;
    }
    length += node_length;
    SeqRegExpNode* seq_node = static_cast<SeqRegExpNode*>(node);
    node = seq_node->on_success();
  }
  return read_backward() ? -length : length;
}


void LoopChoiceNode::AddLoopAlternative(GuardedAlternative alt) {
  DCHECK_NULL(loop_node_);
  AddAlternative(alt);
  loop_node_ = alt.node();
}


void LoopChoiceNode::AddContinueAlternative(GuardedAlternative alt) {
  DCHECK_NULL(continue_node_);
  AddAlternative(alt);
  continue_node_ = alt.node();
}


void LoopChoiceNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  if (trace->stop_node() == this) {
    // Back edge of greedy optimized loop node graph.
    int text_length =
        GreedyLoopTextLengthForAlternative(&(alternatives_->at(0)));
    DCHECK_NE(kNodeIsTooComplexForGreedyLoops, text_length);
    // Update the counter-based backtracking info on the stack.  This is an
    // optimization for greedy loops (see below).
    DCHECK(trace->cp_offset() == text_length);
    macro_assembler->AdvanceCurrentPosition(text_length);
    macro_assembler->GoTo(trace->loop_label());
    return;
  }
  DCHECK_NULL(trace->stop_node());
  if (!trace->is_trivial()) {
    trace->Flush(compiler, this);
    return;
  }
  ChoiceNode::Emit(compiler, trace);
}


int ChoiceNode::CalculatePreloadCharacters(RegExpCompiler* compiler,
                                           int eats_at_least) {
  int preload_characters = Min(4, eats_at_least);
  DCHECK_LE(preload_characters, 4);
  if (compiler->macro_assembler()->CanReadUnaligned()) {
    bool one_byte = compiler->one_byte();
    if (one_byte) {
      // We can't preload 3 characters because there is no machine instruction
      // to do that.  We can't just load 4 because we could be reading
      // beyond the end of the string, which could cause a memory fault.
      if (preload_characters == 3) preload_characters = 2;
    } else {
      if (preload_characters > 2) preload_characters = 2;
    }
  } else {
    if (preload_characters > 1) preload_characters = 1;
  }
  return preload_characters;
}


// This class is used when generating the alternatives in a choice node.  It
// records the way the alternative is being code generated.
class AlternativeGeneration: public Malloced {
 public:
  AlternativeGeneration()
      : possible_success(),
        expects_preload(false),
        after(),
        quick_check_details() { }
  Label possible_success;
  bool expects_preload;
  Label after;
  QuickCheckDetails quick_check_details;
};


// Creates a list of AlternativeGenerations.  If the list has a reasonable
// size then it is on the stack, otherwise the excess is on the heap.
class AlternativeGenerationList {
 public:
  AlternativeGenerationList(int count, Zone* zone)
      : alt_gens_(count, zone) {
    for (int i = 0; i < count && i < kAFew; i++) {
      alt_gens_.Add(a_few_alt_gens_ + i, zone);
    }
    for (int i = kAFew; i < count; i++) {
      alt_gens_.Add(new AlternativeGeneration(), zone);
    }
  }
  ~AlternativeGenerationList() {
    for (int i = kAFew; i < alt_gens_.length(); i++) {
      delete alt_gens_[i];
      alt_gens_[i] = nullptr;
    }
  }

  AlternativeGeneration* at(int i) {
    return alt_gens_[i];
  }

 private:
  static const int kAFew = 10;
  ZoneList<AlternativeGeneration*> alt_gens_;
  AlternativeGeneration a_few_alt_gens_[kAFew];
};


static const uc32 kRangeEndMarker = 0x110000;

// The '2' variant is has inclusive from and exclusive to.
// This covers \s as defined in ECMA-262 5.1, 15.10.2.12,
// which include WhiteSpace (7.2) or LineTerminator (7.3) values.
static const int kSpaceRanges[] = {
    '\t',   '\r' + 1, ' ',    ' ' + 1, 0x00A0, 0x00A1, 0x1680,
    0x1681, 0x2000,   0x200B, 0x2028,  0x202A, 0x202F, 0x2030,
    0x205F, 0x2060,   0x3000, 0x3001,  0xFEFF, 0xFF00, kRangeEndMarker};
static const int kSpaceRangeCount = arraysize(kSpaceRanges);

static const int kWordRanges[] = {
    '0', '9' + 1, 'A', 'Z' + 1, '_', '_' + 1, 'a', 'z' + 1, kRangeEndMarker};
static const int kWordRangeCount = arraysize(kWordRanges);
static const int kDigitRanges[] = {'0', '9' + 1, kRangeEndMarker};
static const int kDigitRangeCount = arraysize(kDigitRanges);
static const int kSurrogateRanges[] = {
    kLeadSurrogateStart, kLeadSurrogateStart + 1, kRangeEndMarker};
static const int kSurrogateRangeCount = arraysize(kSurrogateRanges);
static const int kLineTerminatorRanges[] = {
    0x000A, 0x000B, 0x000D, 0x000E, 0x2028, 0x202A, kRangeEndMarker};
static const int kLineTerminatorRangeCount = arraysize(kLineTerminatorRanges);

void BoyerMoorePositionInfo::Set(int character) {
  SetInterval(Interval(character, character));
}


void BoyerMoorePositionInfo::SetInterval(const Interval& interval) {
  s_ = AddRange(s_, kSpaceRanges, kSpaceRangeCount, interval);
  w_ = AddRange(w_, kWordRanges, kWordRangeCount, interval);
  d_ = AddRange(d_, kDigitRanges, kDigitRangeCount, interval);
  surrogate_ =
      AddRange(surrogate_, kSurrogateRanges, kSurrogateRangeCount, interval);
  if (interval.to() - interval.from() >= kMapSize - 1) {
    if (map_count_ != kMapSize) {
      map_count_ = kMapSize;
      for (int i = 0; i < kMapSize; i++) map_->at(i) = true;
    }
    return;
  }
  for (int i = interval.from(); i <= interval.to(); i++) {
    int mod_character = (i & kMask);
    if (!map_->at(mod_character)) {
      map_count_++;
      map_->at(mod_character) = true;
    }
    if (map_count_ == kMapSize) return;
  }
}


void BoyerMoorePositionInfo::SetAll() {
  s_ = w_ = d_ = kLatticeUnknown;
  if (map_count_ != kMapSize) {
    map_count_ = kMapSize;
    for (int i = 0; i < kMapSize; i++) map_->at(i) = true;
  }
}


BoyerMooreLookahead::BoyerMooreLookahead(
    int length, RegExpCompiler* compiler, Zone* zone)
    : length_(length),
      compiler_(compiler) {
  if (compiler->one_byte()) {
    max_char_ = String::kMaxOneByteCharCode;
  } else {
    max_char_ = String::kMaxUtf16CodeUnit;
  }
  bitmaps_ = new(zone) ZoneList<BoyerMoorePositionInfo*>(length, zone);
  for (int i = 0; i < length; i++) {
    bitmaps_->Add(new(zone) BoyerMoorePositionInfo(zone), zone);
  }
}


// Find the longest range of lookahead that has the fewest number of different
// characters that can occur at a given position.  Since we are optimizing two
// different parameters at once this is a tradeoff.
bool BoyerMooreLookahead::FindWorthwhileInterval(int* from, int* to) {
  int biggest_points = 0;
  // If more than 32 characters out of 128 can occur it is unlikely that we can
  // be lucky enough to step forwards much of the time.
  const int kMaxMax = 32;
  for (int max_number_of_chars = 4;
       max_number_of_chars < kMaxMax;
       max_number_of_chars *= 2) {
    biggest_points =
        FindBestInterval(max_number_of_chars, biggest_points, from, to);
  }
  if (biggest_points == 0) return false;
  return true;
}


// Find the highest-points range between 0 and length_ where the character
// information is not too vague.  'Too vague' means that there are more than
// max_number_of_chars that can occur at this position.  Calculates the number
// of points as the product of width-of-the-range and
// probability-of-finding-one-of-the-characters, where the probability is
// calculated using the frequency distribution of the sample subject string.
int BoyerMooreLookahead::FindBestInterval(
    int max_number_of_chars, int old_biggest_points, int* from, int* to) {
  int biggest_points = old_biggest_points;
  static const int kSize = RegExpMacroAssembler::kTableSize;
  for (int i = 0; i < length_; ) {
    while (i < length_ && Count(i) > max_number_of_chars) i++;
    if (i == length_) break;
    int remembered_from = i;
    bool union_map[kSize];
    for (int j = 0; j < kSize; j++) union_map[j] = false;
    while (i < length_ && Count(i) <= max_number_of_chars) {
      BoyerMoorePositionInfo* map = bitmaps_->at(i);
      for (int j = 0; j < kSize; j++) union_map[j] |= map->at(j);
      i++;
    }
    int frequency = 0;
    for (int j = 0; j < kSize; j++) {
      if (union_map[j]) {
        // Add 1 to the frequency to give a small per-character boost for
        // the cases where our sampling is not good enough and many
        // characters have a frequency of zero.  This means the frequency
        // can theoretically be up to 2*kSize though we treat it mostly as
        // a fraction of kSize.
        frequency += compiler_->frequency_collator()->Frequency(j) + 1;
      }
    }
    // We use the probability of skipping times the distance we are skipping to
    // judge the effectiveness of this.  Actually we have a cut-off:  By
    // dividing by 2 we switch off the skipping if the probability of skipping
    // is less than 50%.  This is because the multibyte mask-and-compare
    // skipping in quickcheck is more likely to do well on this case.
    bool in_quickcheck_range =
        ((i - remembered_from < 4) ||
         (compiler_->one_byte() ? remembered_from <= 4 : remembered_from <= 2));
    // Called 'probability' but it is only a rough estimate and can actually
    // be outside the 0-kSize range.
    int probability = (in_quickcheck_range ? kSize / 2 : kSize) - frequency;
    int points = (i - remembered_from) * probability;
    if (points > biggest_points) {
      *from = remembered_from;
      *to = i - 1;
      biggest_points = points;
    }
  }
  return biggest_points;
}


// Take all the characters that will not prevent a successful match if they
// occur in the subject string in the range between min_lookahead and
// max_lookahead (inclusive) measured from the current position.  If the
// character at max_lookahead offset is not one of these characters, then we
// can safely skip forwards by the number of characters in the range.
int BoyerMooreLookahead::GetSkipTable(int min_lookahead,
                                      int max_lookahead,
                                      Handle<ByteArray> boolean_skip_table) {
  const int kSize = RegExpMacroAssembler::kTableSize;

  const int kSkipArrayEntry = 0;
  const int kDontSkipArrayEntry = 1;

  for (int i = 0; i < kSize; i++) {
    boolean_skip_table->set(i, kSkipArrayEntry);
  }
  int skip = max_lookahead + 1 - min_lookahead;

  for (int i = max_lookahead; i >= min_lookahead; i--) {
    BoyerMoorePositionInfo* map = bitmaps_->at(i);
    for (int j = 0; j < kSize; j++) {
      if (map->at(j)) {
        boolean_skip_table->set(j, kDontSkipArrayEntry);
      }
    }
  }

  return skip;
}


// See comment above on the implementation of GetSkipTable.
void BoyerMooreLookahead::EmitSkipInstructions(RegExpMacroAssembler* masm) {
  const int kSize = RegExpMacroAssembler::kTableSize;

  int min_lookahead = 0;
  int max_lookahead = 0;

  if (!FindWorthwhileInterval(&min_lookahead, &max_lookahead)) return;

  bool found_single_character = false;
  int single_character = 0;
  for (int i = max_lookahead; i >= min_lookahead; i--) {
    BoyerMoorePositionInfo* map = bitmaps_->at(i);
    if (map->map_count() > 1 ||
        (found_single_character && map->map_count() != 0)) {
      found_single_character = false;
      break;
    }
    for (int j = 0; j < kSize; j++) {
      if (map->at(j)) {
        found_single_character = true;
        single_character = j;
        break;
      }
    }
  }

  int lookahead_width = max_lookahead + 1 - min_lookahead;

  if (found_single_character && lookahead_width == 1 && max_lookahead < 3) {
    // The mask-compare can probably handle this better.
    return;
  }

  if (found_single_character) {
    Label cont, again;
    masm->Bind(&again);
    masm->LoadCurrentCharacter(max_lookahead, &cont, true);
    if (max_char_ > kSize) {
      masm->CheckCharacterAfterAnd(single_character,
                                   RegExpMacroAssembler::kTableMask,
                                   &cont);
    } else {
      masm->CheckCharacter(single_character, &cont);
    }
    masm->AdvanceCurrentPosition(lookahead_width);
    masm->GoTo(&again);
    masm->Bind(&cont);
    return;
  }

  Factory* factory = masm->isolate()->factory();
  Handle<ByteArray> boolean_skip_table = factory->NewByteArray(kSize, TENURED);
  int skip_distance = GetSkipTable(
      min_lookahead, max_lookahead, boolean_skip_table);
  DCHECK_NE(0, skip_distance);

  Label cont, again;
  masm->Bind(&again);
  masm->LoadCurrentCharacter(max_lookahead, &cont, true);
  masm->CheckBitInTable(boolean_skip_table, &cont);
  masm->AdvanceCurrentPosition(skip_distance);
  masm->GoTo(&again);
  masm->Bind(&cont);
}


/* Code generation for choice nodes.
 *
 * We generate quick checks that do a mask and compare to eliminate a
 * choice.  If the quick check succeeds then it jumps to the continuation to
 * do slow checks and check subsequent nodes.  If it fails (the common case)
 * it falls through to the next choice.
 *
 * Here is the desired flow graph.  Nodes directly below each other imply
 * fallthrough.  Alternatives 1 and 2 have quick checks.  Alternative
 * 3 doesn't have a quick check so we have to call the slow check.
 * Nodes are marked Qn for quick checks and Sn for slow checks.  The entire
 * regexp continuation is generated directly after the Sn node, up to the
 * next GoTo if we decide to reuse some already generated code.  Some
 * nodes expect preload_characters to be preloaded into the current
 * character register.  R nodes do this preloading.  Vertices are marked
 * F for failures and S for success (possible success in the case of quick
 * nodes).  L, V, < and > are used as arrow heads.
 *
 * ----------> R
 *             |
 *             V
 *            Q1 -----> S1
 *             |   S   /
 *            F|      /
 *             |    F/
 *             |    /
 *             |   R
 *             |  /
 *             V L
 *            Q2 -----> S2
 *             |   S   /
 *            F|      /
 *             |    F/
 *             |    /
 *             |   R
 *             |  /
 *             V L
 *            S3
 *             |
 *            F|
 *             |
 *             R
 *             |
 * backtrack   V
 * <----------Q4
 *   \    F    |
 *    \        |S
 *     \   F   V
 *      \-----S4
 *
 * For greedy loops we push the current position, then generate the code that
 * eats the input specially in EmitGreedyLoop.  The other choice (the
 * continuation) is generated by the normal code in EmitChoices, and steps back
 * in the input to the starting position when it fails to match.  The loop code
 * looks like this (U is the unwind code that steps back in the greedy loop).
 *
 *              _____
 *             /     \
 *             V     |
 * ----------> S1    |
 *            /|     |
 *           / |S    |
 *         F/  \_____/
 *         /
 *        |<-----
 *        |      \
 *        V       |S
 *        Q2 ---> U----->backtrack
 *        |  F   /
 *       S|     /
 *        V  F /
 *        S2--/
 */

GreedyLoopState::GreedyLoopState(bool not_at_start) {
  counter_backtrack_trace_.set_backtrack(&label_);
  if (not_at_start) counter_backtrack_trace_.set_at_start(Trace::FALSE_VALUE);
}


void ChoiceNode::AssertGuardsMentionRegisters(Trace* trace) {
#ifdef DEBUG
  int choice_count = alternatives_->length();
  for (int i = 0; i < choice_count - 1; i++) {
    GuardedAlternative alternative = alternatives_->at(i);
    ZoneList<Guard*>* guards = alternative.guards();
    int guard_count = (guards == nullptr) ? 0 : guards->length();
    for (int j = 0; j < guard_count; j++) {
      DCHECK(!trace->mentions_reg(guards->at(j)->reg()));
    }
  }
#endif
}


void ChoiceNode::SetUpPreLoad(RegExpCompiler* compiler,
                              Trace* current_trace,
                              PreloadState* state) {
    if (state->eats_at_least_ == PreloadState::kEatsAtLeastNotYetInitialized) {
      // Save some time by looking at most one machine word ahead.
      state->eats_at_least_ =
          EatsAtLeast(compiler->one_byte() ? 4 : 2, kRecursionBudget,
                      current_trace->at_start() == Trace::FALSE_VALUE);
    }
    state->preload_characters_ =
        CalculatePreloadCharacters(compiler, state->eats_at_least_);

    state->preload_is_current_ =
        (current_trace->characters_preloaded() == state->preload_characters_);
    state->preload_has_checked_bounds_ = state->preload_is_current_;
}


void ChoiceNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  int choice_count = alternatives_->length();

  if (choice_count == 1 && alternatives_->at(0).guards() == nullptr) {
    alternatives_->at(0).node()->Emit(compiler, trace);
    return;
  }

  AssertGuardsMentionRegisters(trace);

  LimitResult limit_result = LimitVersions(compiler, trace);
  if (limit_result == DONE) return;
  DCHECK(limit_result == CONTINUE);

  // For loop nodes we already flushed (see LoopChoiceNode::Emit), but for
  // other choice nodes we only flush if we are out of code size budget.
  if (trace->flush_budget() == 0 && trace->actions() != nullptr) {
    trace->Flush(compiler, this);
    return;
  }

  RecursionCheck rc(compiler);

  PreloadState preload;
  preload.init();
  GreedyLoopState greedy_loop_state(not_at_start());

  int text_length = GreedyLoopTextLengthForAlternative(&alternatives_->at(0));
  AlternativeGenerationList alt_gens(choice_count, zone());

  if (choice_count > 1 && text_length != kNodeIsTooComplexForGreedyLoops) {
    trace = EmitGreedyLoop(compiler,
                           trace,
                           &alt_gens,
                           &preload,
                           &greedy_loop_state,
                           text_length);
  } else {
    // TODO(erikcorry): Delete this.  We don't need this label, but it makes us
    // match the traces produced pre-cleanup.
    Label second_choice;
    compiler->macro_assembler()->Bind(&second_choice);

    preload.eats_at_least_ = EmitOptimizedUnanchoredSearch(compiler, trace);

    EmitChoices(compiler,
                &alt_gens,
                0,
                trace,
                &preload);
  }

  // At this point we need to generate slow checks for the alternatives where
  // the quick check was inlined.  We can recognize these because the associated
  // label was bound.
  int new_flush_budget = trace->flush_budget() / choice_count;
  for (int i = 0; i < choice_count; i++) {
    AlternativeGeneration* alt_gen = alt_gens.at(i);
    Trace new_trace(*trace);
    // If there are actions to be flushed we have to limit how many times
    // they are flushed.  Take the budget of the parent trace and distribute
    // it fairly amongst the children.
    if (new_trace.actions() != nullptr) {
      new_trace.set_flush_budget(new_flush_budget);
    }
    bool next_expects_preload =
        i == choice_count - 1 ? false : alt_gens.at(i + 1)->expects_preload;
    EmitOutOfLineContinuation(compiler,
                              &new_trace,
                              alternatives_->at(i),
                              alt_gen,
                              preload.preload_characters_,
                              next_expects_preload);
  }
}


Trace* ChoiceNode::EmitGreedyLoop(RegExpCompiler* compiler,
                                  Trace* trace,
                                  AlternativeGenerationList* alt_gens,
                                  PreloadState* preload,
                                  GreedyLoopState* greedy_loop_state,
                                  int text_length) {
  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  // Here we have special handling for greedy loops containing only text nodes
  // and other simple nodes.  These are handled by pushing the current
  // position on the stack and then incrementing the current position each
  // time around the switch.  On backtrack we decrement the current position
  // and check it against the pushed value.  This avoids pushing backtrack
  // information for each iteration of the loop, which could take up a lot of
  // space.
  DCHECK(trace->stop_node() == nullptr);
  macro_assembler->PushCurrentPosition();
  Label greedy_match_failed;
  Trace greedy_match_trace;
  if (not_at_start()) greedy_match_trace.set_at_start(Trace::FALSE_VALUE);
  greedy_match_trace.set_backtrack(&greedy_match_failed);
  Label loop_label;
  macro_assembler->Bind(&loop_label);
  greedy_match_trace.set_stop_node(this);
  greedy_match_trace.set_loop_label(&loop_label);
  alternatives_->at(0).node()->Emit(compiler, &greedy_match_trace);
  macro_assembler->Bind(&greedy_match_failed);

  Label second_choice;  // For use in greedy matches.
  macro_assembler->Bind(&second_choice);

  Trace* new_trace = greedy_loop_state->counter_backtrack_trace();

  EmitChoices(compiler,
              alt_gens,
              1,
              new_trace,
              preload);

  macro_assembler->Bind(greedy_loop_state->label());
  // If we have unwound to the bottom then backtrack.
  macro_assembler->CheckGreedyLoop(trace->backtrack());
  // Otherwise try the second priority at an earlier position.
  macro_assembler->AdvanceCurrentPosition(-text_length);
  macro_assembler->GoTo(&second_choice);
  return new_trace;
}

int ChoiceNode::EmitOptimizedUnanchoredSearch(RegExpCompiler* compiler,
                                              Trace* trace) {
  int eats_at_least = PreloadState::kEatsAtLeastNotYetInitialized;
  if (alternatives_->length() != 2) return eats_at_least;

  GuardedAlternative alt1 = alternatives_->at(1);
  if (alt1.guards() != nullptr && alt1.guards()->length() != 0) {
    return eats_at_least;
  }
  RegExpNode* eats_anything_node = alt1.node();
  if (eats_anything_node->GetSuccessorOfOmnivorousTextNode(compiler) != this) {
    return eats_at_least;
  }

  // Really we should be creating a new trace when we execute this function,
  // but there is no need, because the code it generates cannot backtrack, and
  // we always arrive here with a trivial trace (since it's the entry to a
  // loop.  That also implies that there are no preloaded characters, which is
  // good, because it means we won't be violating any assumptions by
  // overwriting those characters with new load instructions.
  DCHECK(trace->is_trivial());

  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  Isolate* isolate = macro_assembler->isolate();
  // At this point we know that we are at a non-greedy loop that will eat
  // any character one at a time.  Any non-anchored regexp has such a
  // loop prepended to it in order to find where it starts.  We look for
  // a pattern of the form ...abc... where we can look 6 characters ahead
  // and step forwards 3 if the character is not one of abc.  Abc need
  // not be atoms, they can be any reasonably limited character class or
  // small alternation.
  BoyerMooreLookahead* bm = bm_info(false);
  if (bm == nullptr) {
    eats_at_least = Min(kMaxLookaheadForBoyerMoore,
                        EatsAtLeast(kMaxLookaheadForBoyerMoore,
                                    kRecursionBudget,
                                    false));
    if (eats_at_least >= 1) {
      bm = new(zone()) BoyerMooreLookahead(eats_at_least,
                                           compiler,
                                           zone());
      GuardedAlternative alt0 = alternatives_->at(0);
      alt0.node()->FillInBMInfo(isolate, 0, kRecursionBudget, bm, false);
    }
  }
  if (bm != nullptr) {
    bm->EmitSkipInstructions(macro_assembler);
  }
  return eats_at_least;
}


void ChoiceNode::EmitChoices(RegExpCompiler* compiler,
                             AlternativeGenerationList* alt_gens,
                             int first_choice,
                             Trace* trace,
                             PreloadState* preload) {
  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  SetUpPreLoad(compiler, trace, preload);

  // For now we just call all choices one after the other.  The idea ultimately
  // is to use the Dispatch table to try only the relevant ones.
  int choice_count = alternatives_->length();

  int new_flush_budget = trace->flush_budget() / choice_count;

  for (int i = first_choice; i < choice_count; i++) {
    bool is_last = i == choice_count - 1;
    bool fall_through_on_failure = !is_last;
    GuardedAlternative alternative = alternatives_->at(i);
    AlternativeGeneration* alt_gen = alt_gens->at(i);
    alt_gen->quick_check_details.set_characters(preload->preload_characters_);
    ZoneList<Guard*>* guards = alternative.guards();
    int guard_count = (guards == nullptr) ? 0 : guards->length();
    Trace new_trace(*trace);
    new_trace.set_characters_preloaded(preload->preload_is_current_ ?
                                         preload->preload_characters_ :
                                         0);
    if (preload->preload_has_checked_bounds_) {
      new_trace.set_bound_checked_up_to(preload->preload_characters_);
    }
    new_trace.quick_check_performed()->Clear();
    if (not_at_start_) new_trace.set_at_start(Trace::FALSE_VALUE);
    if (!is_last) {
      new_trace.set_backtrack(&alt_gen->after);
    }
    alt_gen->expects_preload = preload->preload_is_current_;
    bool generate_full_check_inline = false;
    if (compiler->optimize() &&
        try_to_emit_quick_check_for_alternative(i == 0) &&
        alternative.node()->EmitQuickCheck(
            compiler, trace, &new_trace, preload->preload_has_checked_bounds_,
            &alt_gen->possible_success, &alt_gen->quick_check_details,
            fall_through_on_failure)) {
      // Quick check was generated for this choice.
      preload->preload_is_current_ = true;
      preload->preload_has_checked_bounds_ = true;
      // If we generated the quick check to fall through on possible success,
      // we now need to generate the full check inline.
      if (!fall_through_on_failure) {
        macro_assembler->Bind(&alt_gen->possible_success);
        new_trace.set_quick_check_performed(&alt_gen->quick_check_details);
        new_trace.set_characters_preloaded(preload->preload_characters_);
        new_trace.set_bound_checked_up_to(preload->preload_characters_);
        generate_full_check_inline = true;
      }
    } else if (alt_gen->quick_check_details.cannot_match()) {
      if (!fall_through_on_failure) {
        macro_assembler->GoTo(trace->backtrack());
      }
      continue;
    } else {
      // No quick check was generated.  Put the full code here.
      // If this is not the first choice then there could be slow checks from
      // previous cases that go here when they fail.  There's no reason to
      // insist that they preload characters since the slow check we are about
      // to generate probably can't use it.
      if (i != first_choice) {
        alt_gen->expects_preload = false;
        new_trace.InvalidateCurrentCharacter();
      }
      generate_full_check_inline = true;
    }
    if (generate_full_check_inline) {
      if (new_trace.actions() != nullptr) {
        new_trace.set_flush_budget(new_flush_budget);
      }
      for (int j = 0; j < guard_count; j++) {
        GenerateGuard(macro_assembler, guards->at(j), &new_trace);
      }
      alternative.node()->Emit(compiler, &new_trace);
      preload->preload_is_current_ = false;
    }
    macro_assembler->Bind(&alt_gen->after);
  }
}


void ChoiceNode::EmitOutOfLineContinuation(RegExpCompiler* compiler,
                                           Trace* trace,
                                           GuardedAlternative alternative,
                                           AlternativeGeneration* alt_gen,
                                           int preload_characters,
                                           bool next_expects_preload) {
  if (!alt_gen->possible_success.is_linked()) return;

  RegExpMacroAssembler* macro_assembler = compiler->macro_assembler();
  macro_assembler->Bind(&alt_gen->possible_success);
  Trace out_of_line_trace(*trace);
  out_of_line_trace.set_characters_preloaded(preload_characters);
  out_of_line_trace.set_quick_check_performed(&alt_gen->quick_check_details);
  if (not_at_start_) out_of_line_trace.set_at_start(Trace::FALSE_VALUE);
  ZoneList<Guard*>* guards = alternative.guards();
  int guard_count = (guards == nullptr) ? 0 : guards->length();
  if (next_expects_preload) {
    Label reload_current_char;
    out_of_line_trace.set_backtrack(&reload_current_char);
    for (int j = 0; j < guard_count; j++) {
      GenerateGuard(macro_assembler, guards->at(j), &out_of_line_trace);
    }
    alternative.node()->Emit(compiler, &out_of_line_trace);
    macro_assembler->Bind(&reload_current_char);
    // Reload the current character, since the next quick check expects that.
    // We don't need to check bounds here because we only get into this
    // code through a quick check which already did the checked load.
    macro_assembler->LoadCurrentCharacter(trace->cp_offset(), nullptr, false,
                                          preload_characters);
    macro_assembler->GoTo(&(alt_gen->after));
  } else {
    out_of_line_trace.set_backtrack(&(alt_gen->after));
    for (int j = 0; j < guard_count; j++) {
      GenerateGuard(macro_assembler, guards->at(j), &out_of_line_trace);
    }
    alternative.node()->Emit(compiler, &out_of_line_trace);
  }
}


void ActionNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  LimitResult limit_result = LimitVersions(compiler, trace);
  if (limit_result == DONE) return;
  DCHECK(limit_result == CONTINUE);

  RecursionCheck rc(compiler);

  switch (action_type_) {
    case STORE_POSITION: {
      Trace::DeferredCapture
          new_capture(data_.u_position_register.reg,
                      data_.u_position_register.is_capture,
                      trace);
      Trace new_trace = *trace;
      new_trace.add_action(&new_capture);
      on_success()->Emit(compiler, &new_trace);
      break;
    }
    case INCREMENT_REGISTER: {
      Trace::DeferredIncrementRegister
          new_increment(data_.u_increment_register.reg);
      Trace new_trace = *trace;
      new_trace.add_action(&new_increment);
      on_success()->Emit(compiler, &new_trace);
      break;
    }
    case SET_REGISTER: {
      Trace::DeferredSetRegister
          new_set(data_.u_store_register.reg, data_.u_store_register.value);
      Trace new_trace = *trace;
      new_trace.add_action(&new_set);
      on_success()->Emit(compiler, &new_trace);
      break;
    }
    case CLEAR_CAPTURES: {
      Trace::DeferredClearCaptures
        new_capture(Interval(data_.u_clear_captures.range_from,
                             data_.u_clear_captures.range_to));
      Trace new_trace = *trace;
      new_trace.add_action(&new_capture);
      on_success()->Emit(compiler, &new_trace);
      break;
    }
    case BEGIN_SUBMATCH:
      if (!trace->is_trivial()) {
        trace->Flush(compiler, this);
      } else {
        assembler->WriteCurrentPositionToRegister(
            data_.u_submatch.current_position_register, 0);
        assembler->WriteStackPointerToRegister(
            data_.u_submatch.stack_pointer_register);
        on_success()->Emit(compiler, trace);
      }
      break;
    case EMPTY_MATCH_CHECK: {
      int start_pos_reg = data_.u_empty_match_check.start_register;
      int stored_pos = 0;
      int rep_reg = data_.u_empty_match_check.repetition_register;
      bool has_minimum = (rep_reg != RegExpCompiler::kNoRegister);
      bool know_dist = trace->GetStoredPosition(start_pos_reg, &stored_pos);
      if (know_dist && !has_minimum && stored_pos == trace->cp_offset()) {
        // If we know we haven't advanced and there is no minimum we
        // can just backtrack immediately.
        assembler->GoTo(trace->backtrack());
      } else if (know_dist && stored_pos < trace->cp_offset()) {
        // If we know we've advanced we can generate the continuation
        // immediately.
        on_success()->Emit(compiler, trace);
      } else if (!trace->is_trivial()) {
        trace->Flush(compiler, this);
      } else {
        Label skip_empty_check;
        // If we have a minimum number of repetitions we check the current
        // number first and skip the empty check if it's not enough.
        if (has_minimum) {
          int limit = data_.u_empty_match_check.repetition_limit;
          assembler->IfRegisterLT(rep_reg, limit, &skip_empty_check);
        }
        // If the match is empty we bail out, otherwise we fall through
        // to the on-success continuation.
        assembler->IfRegisterEqPos(data_.u_empty_match_check.start_register,
                                   trace->backtrack());
        assembler->Bind(&skip_empty_check);
        on_success()->Emit(compiler, trace);
      }
      break;
    }
    case POSITIVE_SUBMATCH_SUCCESS: {
      if (!trace->is_trivial()) {
        trace->Flush(compiler, this);
        return;
      }
      assembler->ReadCurrentPositionFromRegister(
          data_.u_submatch.current_position_register);
      assembler->ReadStackPointerFromRegister(
          data_.u_submatch.stack_pointer_register);
      int clear_register_count = data_.u_submatch.clear_register_count;
      if (clear_register_count == 0) {
        on_success()->Emit(compiler, trace);
        return;
      }
      int clear_registers_from = data_.u_submatch.clear_register_from;
      Label clear_registers_backtrack;
      Trace new_trace = *trace;
      new_trace.set_backtrack(&clear_registers_backtrack);
      on_success()->Emit(compiler, &new_trace);

      assembler->Bind(&clear_registers_backtrack);
      int clear_registers_to = clear_registers_from + clear_register_count - 1;
      assembler->ClearRegisters(clear_registers_from, clear_registers_to);

      DCHECK(trace->backtrack() == nullptr);
      assembler->Backtrack();
      return;
    }
    default:
      UNREACHABLE();
  }
}


void BackReferenceNode::Emit(RegExpCompiler* compiler, Trace* trace) {
  RegExpMacroAssembler* assembler = compiler->macro_assembler();
  if (!trace->is_trivial()) {
    trace->Flush(compiler, this);
    return;
  }

  LimitResult limit_result = LimitVersions(compiler, trace);
  if (limit_result == DONE) return;
  DCHECK(limit_result == CONTINUE);

  RecursionCheck rc(compiler);

  DCHECK_EQ(start_reg_ + 1, end_reg_);
  if (IgnoreCase(flags_)) {
    assembler->CheckNotBackReferenceIgnoreCase(
        start_reg_, read_backward(), IsUnicode(flags_), trace->backtrack());
  } else {
    assembler->CheckNotBackReference(start_reg_, read_backward(),
                                     trace->backtrack());
  }
  // We are going to advance backward, so we may end up at the start.
  if (read_backward()) trace->set_at_start(Trace::UNKNOWN);

  // Check that the back reference does not end inside a surrogate pair.
  if (IsUnicode(flags_) && !compiler->one_byte()) {
    assembler->CheckNotInSurrogatePair(trace->cp_offset(), trace->backtrack());
  }
  on_success()->Emit(compiler, trace);
}


// -------------------------------------------------------------------
// Dot/dotty output


#ifdef DEBUG


class DotPrinter: public NodeVisitor {
 public:
  DotPrinter(std::ostream& os, bool ignore_case)  // NOLINT
      : os_(os),
        ignore_case_(ignore_case) {}
  void PrintNode(const char* label, RegExpNode* node);
  void Visit(RegExpNode* node);
  void PrintAttributes(RegExpNode* from);
  void PrintOnFailure(RegExpNode* from, RegExpNode* to);
#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that);
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
 private:
  std::ostream& os_;
  bool ignore_case_;
};


void DotPrinter::PrintNode(const char* label, RegExpNode* node) {
  os_ << "digraph G {\n  graph [label=\"";
  for (int i = 0; label[i]; i++) {
    switch (label[i]) {
      case '\\':
        os_ << "\\\\";
        break;
      case '"':
        os_ << "\"";
        break;
      default:
        os_ << label[i];
        break;
    }
  }
  os_ << "\"];\n";
  Visit(node);
  os_ << "}" << std::endl;
}


void DotPrinter::Visit(RegExpNode* node) {
  if (node->info()->visited) return;
  node->info()->visited = true;
  node->Accept(this);
}


void DotPrinter::PrintOnFailure(RegExpNode* from, RegExpNode* on_failure) {
  os_ << "  n" << from << " -> n" << on_failure << " [style=dotted];\n";
  Visit(on_failure);
}


class TableEntryBodyPrinter {
 public:
  TableEntryBodyPrinter(std::ostream& os, ChoiceNode* choice)  // NOLINT
      : os_(os),
        choice_(choice) {}
  void Call(uc16 from, DispatchTable::Entry entry) {
    OutSet* out_set = entry.out_set();
    for (unsigned i = 0; i < OutSet::kFirstLimit; i++) {
      if (out_set->Get(i)) {
        os_ << "    n" << choice() << ":s" << from << "o" << i << " -> n"
            << choice()->alternatives()->at(i).node() << ";\n";
      }
    }
  }
 private:
  ChoiceNode* choice() { return choice_; }
  std::ostream& os_;
  ChoiceNode* choice_;
};


class TableEntryHeaderPrinter {
 public:
  explicit TableEntryHeaderPrinter(std::ostream& os)  // NOLINT
      : first_(true),
        os_(os) {}
  void Call(uc16 from, DispatchTable::Entry entry) {
    if (first_) {
      first_ = false;
    } else {
      os_ << "|";
    }
    os_ << "{\\" << AsUC16(from) << "-\\" << AsUC16(entry.to()) << "|{";
    OutSet* out_set = entry.out_set();
    int priority = 0;
    for (unsigned i = 0; i < OutSet::kFirstLimit; i++) {
      if (out_set->Get(i)) {
        if (priority > 0) os_ << "|";
        os_ << "<s" << from << "o" << i << "> " << priority;
        priority++;
      }
    }
    os_ << "}}";
  }

 private:
  bool first_;
  std::ostream& os_;
};


class AttributePrinter {
 public:
  explicit AttributePrinter(std::ostream& os)  // NOLINT
      : os_(os),
        first_(true) {}
  void PrintSeparator() {
    if (first_) {
      first_ = false;
    } else {
      os_ << "|";
    }
  }
  void PrintBit(const char* name, bool value) {
    if (!value) return;
    PrintSeparator();
    os_ << "{" << name << "}";
  }
  void PrintPositive(const char* name, int value) {
    if (value < 0) return;
    PrintSeparator();
    os_ << "{" << name << "|" << value << "}";
  }

 private:
  std::ostream& os_;
  bool first_;
};


void DotPrinter::PrintAttributes(RegExpNode* that) {
  os_ << "  a" << that << " [shape=Mrecord, color=grey, fontcolor=grey, "
      << "margin=0.1, fontsize=10, label=\"{";
  AttributePrinter printer(os_);
  NodeInfo* info = that->info();
  printer.PrintBit("NI", info->follows_newline_interest);
  printer.PrintBit("WI", info->follows_word_interest);
  printer.PrintBit("SI", info->follows_start_interest);
  Label* label = that->label();
  if (label->is_bound())
    printer.PrintPositive("@", label->pos());
  os_ << "}\"];\n"
      << "  a" << that << " -> n" << that
      << " [style=dashed, color=grey, arrowhead=none];\n";
}


static const bool kPrintDispatchTable = false;
void DotPrinter::VisitChoice(ChoiceNode* that) {
  if (kPrintDispatchTable) {
    os_ << "  n" << that << " [shape=Mrecord, label=\"";
    TableEntryHeaderPrinter header_printer(os_);
    that->GetTable(ignore_case_)->ForEach(&header_printer);
    os_ << "\"]\n";
    PrintAttributes(that);
    TableEntryBodyPrinter body_printer(os_, that);
    that->GetTable(ignore_case_)->ForEach(&body_printer);
  } else {
    os_ << "  n" << that << " [shape=Mrecord, label=\"?\"];\n";
    for (int i = 0; i < that->alternatives()->length(); i++) {
      GuardedAlternative alt = that->alternatives()->at(i);
      os_ << "  n" << that << " -> n" << alt.node();
    }
  }
  for (int i = 0; i < that->alternatives()->length(); i++) {
    GuardedAlternative alt = that->alternatives()->at(i);
    alt.node()->Accept(this);
  }
}


void DotPrinter::VisitText(TextNode* that) {
  Zone* zone = that->zone();
  os_ << "  n" << that << " [label=\"";
  for (int i = 0; i < that->elements()->length(); i++) {
    if (i > 0) os_ << " ";
    TextElement elm = that->elements()->at(i);
    switch (elm.text_type()) {
      case TextElement::ATOM: {
        Vector<const uc16> data = elm.atom()->data();
        for (int i = 0; i < data.length(); i++) {
          os_ << static_cast<char>(data[i]);
        }
        break;
      }
      case TextElement::CHAR_CLASS: {
        RegExpCharacterClass* node = elm.char_class();
        os_ << "[";
        if (node->is_negated()) os_ << "^";
        for (int j = 0; j < node->ranges(zone)->length(); j++) {
          CharacterRange range = node->ranges(zone)->at(j);
          os_ << AsUC16(range.from()) << "-" << AsUC16(range.to());
        }
        os_ << "]";
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  os_ << "\", shape=box, peripheries=2];\n";
  PrintAttributes(that);
  os_ << "  n" << that << " -> n" << that->on_success() << ";\n";
  Visit(that->on_success());
}


void DotPrinter::VisitBackReference(BackReferenceNode* that) {
  os_ << "  n" << that << " [label=\"$" << that->start_register() << "..$"
      << that->end_register() << "\", shape=doubleoctagon];\n";
  PrintAttributes(that);
  os_ << "  n" << that << " -> n" << that->on_success() << ";\n";
  Visit(that->on_success());
}


void DotPrinter::VisitEnd(EndNode* that) {
  os_ << "  n" << that << " [style=bold, shape=point];\n";
  PrintAttributes(that);
}


void DotPrinter::VisitAssertion(AssertionNode* that) {
  os_ << "  n" << that << " [";
  switch (that->assertion_type()) {
    case AssertionNode::AT_END:
      os_ << "label=\"$\", shape=septagon";
      break;
    case AssertionNode::AT_START:
      os_ << "label=\"^\", shape=septagon";
      break;
    case AssertionNode::AT_BOUNDARY:
      os_ << "label=\"\\b\", shape=septagon";
      break;
    case AssertionNode::AT_NON_BOUNDARY:
      os_ << "label=\"\\B\", shape=septagon";
      break;
    case AssertionNode::AFTER_NEWLINE:
      os_ << "label=\"(?<=\\n)\", shape=septagon";
      break;
  }
  os_ << "];\n";
  PrintAttributes(that);
  RegExpNode* successor = that->on_success();
  os_ << "  n" << that << " -> n" << successor << ";\n";
  Visit(successor);
}


void DotPrinter::VisitAction(ActionNode* that) {
  os_ << "  n" << that << " [";
  switch (that->action_type_) {
    case ActionNode::SET_REGISTER:
      os_ << "label=\"$" << that->data_.u_store_register.reg
          << ":=" << that->data_.u_store_register.value << "\", shape=octagon";
      break;
    case ActionNode::INCREMENT_REGISTER:
      os_ << "label=\"$" << that->data_.u_increment_register.reg
          << "++\", shape=octagon";
      break;
    case ActionNode::STORE_POSITION:
      os_ << "label=\"$" << that->data_.u_position_register.reg
          << ":=$pos\", shape=octagon";
      break;
    case ActionNode::BEGIN_SUBMATCH:
      os_ << "label=\"$" << that->data_.u_submatch.current_position_register
          << ":=$pos,begin\", shape=septagon";
      break;
    case ActionNode::POSITIVE_SUBMATCH_SUCCESS:
      os_ << "label=\"escape\", shape=septagon";
      break;
    case ActionNode::EMPTY_MATCH_CHECK:
      os_ << "label=\"$" << that->data_.u_empty_match_check.start_register
          << "=$pos?,$" << that->data_.u_empty_match_check.repetition_register
          << "<" << that->data_.u_empty_match_check.repetition_limit
          << "?\", shape=septagon";
      break;
    case ActionNode::CLEAR_CAPTURES: {
      os_ << "label=\"clear $" << that->data_.u_clear_captures.range_from
          << " to $" << that->data_.u_clear_captures.range_to
          << "\", shape=septagon";
      break;
    }
  }
  os_ << "];\n";
  PrintAttributes(that);
  RegExpNode* successor = that->on_success();
  os_ << "  n" << that << " -> n" << successor << ";\n";
  Visit(successor);
}


class DispatchTableDumper {
 public:
  explicit DispatchTableDumper(std::ostream& os) : os_(os) {}
  void Call(uc16 key, DispatchTable::Entry entry);
 private:
  std::ostream& os_;
};


void DispatchTableDumper::Call(uc16 key, DispatchTable::Entry entry) {
  os_ << "[" << AsUC16(key) << "-" << AsUC16(entry.to()) << "]: {";
  OutSet* set = entry.out_set();
  bool first = true;
  for (unsigned i = 0; i < OutSet::kFirstLimit; i++) {
    if (set->Get(i)) {
      if (first) {
        first = false;
      } else {
        os_ << ", ";
      }
      os_ << i;
    }
  }
  os_ << "}\n";
}


void DispatchTable::Dump() {
  OFStream os(stderr);
  DispatchTableDumper dumper(os);
  tree()->ForEach(&dumper);
}


void RegExpEngine::DotPrint(const char* label,
                            RegExpNode* node,
                            bool ignore_case) {
  OFStream os(stdout);
  DotPrinter printer(os, ignore_case);
  printer.PrintNode(label, node);
}


#endif  // DEBUG


// -------------------------------------------------------------------
// Tree to graph conversion

RegExpNode* RegExpAtom::ToNode(RegExpCompiler* compiler,
                               RegExpNode* on_success) {
  ZoneList<TextElement>* elms =
      new(compiler->zone()) ZoneList<TextElement>(1, compiler->zone());
  elms->Add(TextElement::Atom(this), compiler->zone());
  return new (compiler->zone())
      TextNode(elms, compiler->read_backward(), on_success);
}


RegExpNode* RegExpText::ToNode(RegExpCompiler* compiler,
                               RegExpNode* on_success) {
  return new (compiler->zone())
      TextNode(elements(), compiler->read_backward(), on_success);
}


static bool CompareInverseRanges(ZoneList<CharacterRange>* ranges,
                                 const int* special_class,
                                 int length) {
  length--;  // Remove final marker.
  DCHECK_EQ(kRangeEndMarker, special_class[length]);
  DCHECK_NE(0, ranges->length());
  DCHECK_NE(0, length);
  DCHECK_NE(0, special_class[0]);
  if (ranges->length() != (length >> 1) + 1) {
    return false;
  }
  CharacterRange range = ranges->at(0);
  if (range.from() != 0) {
    return false;
  }
  for (int i = 0; i < length; i += 2) {
    if (special_class[i] != (range.to() + 1)) {
      return false;
    }
    range = ranges->at((i >> 1) + 1);
    if (special_class[i+1] != range.from()) {
      return false;
    }
  }
  if (range.to() != String::kMaxCodePoint) {
    return false;
  }
  return true;
}


static bool CompareRanges(ZoneList<CharacterRange>* ranges,
                          const int* special_class,
                          int length) {
  length--;  // Remove final marker.
  DCHECK_EQ(kRangeEndMarker, special_class[length]);
  if (ranges->length() * 2 != length) {
    return false;
  }
  for (int i = 0; i < length; i += 2) {
    CharacterRange range = ranges->at(i >> 1);
    if (range.from() != special_class[i] ||
        range.to() != special_class[i + 1] - 1) {
      return false;
    }
  }
  return true;
}


bool RegExpCharacterClass::is_standard(Zone* zone) {
  // TODO(lrn): Remove need for this function, by not throwing away information
  // along the way.
  if (is_negated()) {
    return false;
  }
  if (set_.is_standard()) {
    return true;
  }
  if (CompareRanges(set_.ranges(zone), kSpaceRanges, kSpaceRangeCount)) {
    set_.set_standard_set_type('s');
    return true;
  }
  if (CompareInverseRanges(set_.ranges(zone), kSpaceRanges, kSpaceRangeCount)) {
    set_.set_standard_set_type('S');
    return true;
  }
  if (CompareInverseRanges(set_.ranges(zone),
                           kLineTerminatorRanges,
                           kLineTerminatorRangeCount)) {
    set_.set_standard_set_type('.');
    return true;
  }
  if (CompareRanges(set_.ranges(zone),
                    kLineTerminatorRanges,
                    kLineTerminatorRangeCount)) {
    set_.set_standard_set_type('n');
    return true;
  }
  if (CompareRanges(set_.ranges(zone), kWordRanges, kWordRangeCount)) {
    set_.set_standard_set_type('w');
    return true;
  }
  if (CompareInverseRanges(set_.ranges(zone), kWordRanges, kWordRangeCount)) {
    set_.set_standard_set_type('W');
    return true;
  }
  return false;
}


UnicodeRangeSplitter::UnicodeRangeSplitter(Zone* zone,
                                           ZoneList<CharacterRange>* base)
    : zone_(zone),
      table_(zone),
      bmp_(nullptr),
      lead_surrogates_(nullptr),
      trail_surrogates_(nullptr),
      non_bmp_(nullptr) {
  // The unicode range splitter categorizes given character ranges into:
  // - Code points from the BMP representable by one code unit.
  // - Code points outside the BMP that need to be split into surrogate pairs.
  // - Lone lead surrogates.
  // - Lone trail surrogates.
  // Lone surrogates are valid code points, even though no actual characters.
  // They require special matching to make sure we do not split surrogate pairs.
  // We use the dispatch table to accomplish this. The base range is split up
  // by the table by the overlay ranges, and the Call callback is used to
  // filter and collect ranges for each category.
  for (int i = 0; i < base->length(); i++) {
    table_.AddRange(base->at(i), kBase, zone_);
  }
  // Add overlay ranges.
  table_.AddRange(CharacterRange::Range(0, kLeadSurrogateStart - 1),
                  kBmpCodePoints, zone_);
  table_.AddRange(CharacterRange::Range(kLeadSurrogateStart, kLeadSurrogateEnd),
                  kLeadSurrogates, zone_);
  table_.AddRange(
      CharacterRange::Range(kTrailSurrogateStart, kTrailSurrogateEnd),
      kTrailSurrogates, zone_);
  table_.AddRange(
      CharacterRange::Range(kTrailSurrogateEnd + 1, kNonBmpStart - 1),
      kBmpCodePoints, zone_);
  table_.AddRange(CharacterRange::Range(kNonBmpStart, kNonBmpEnd),
                  kNonBmpCodePoints, zone_);
  table_.ForEach(this);
}


void UnicodeRangeSplitter::Call(uc32 from, DispatchTable::Entry entry) {
  OutSet* outset = entry.out_set();
  if (!outset->Get(kBase)) return;
  ZoneList<CharacterRange>** target = nullptr;
  if (outset->Get(kBmpCodePoints)) {
    target = &bmp_;
  } else if (outset->Get(kLeadSurrogates)) {
    target = &lead_surrogates_;
  } else if (outset->Get(kTrailSurrogates)) {
    target = &trail_surrogates_;
  } else {
    DCHECK(outset->Get(kNonBmpCodePoints));
    target = &non_bmp_;
  }
  if (*target == nullptr)
    *target = new (zone_) ZoneList<CharacterRange>(2, zone_);
  (*target)->Add(CharacterRange::Range(entry.from(), entry.to()), zone_);
}

void AddBmpCharacters(RegExpCompiler* compiler, ChoiceNode* result,
                      RegExpNode* on_success, UnicodeRangeSplitter* splitter) {
  ZoneList<CharacterRange>* bmp = splitter->bmp();
  if (bmp == nullptr) return;
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  result->AddAlternative(GuardedAlternative(TextNode::CreateForCharacterRanges(
      compiler->zone(), bmp, compiler->read_backward(), on_success,
      default_flags)));
}

void AddNonBmpSurrogatePairs(RegExpCompiler* compiler, ChoiceNode* result,
                             RegExpNode* on_success,
                             UnicodeRangeSplitter* splitter) {
  ZoneList<CharacterRange>* non_bmp = splitter->non_bmp();
  if (non_bmp == nullptr) return;
  DCHECK(!compiler->one_byte());
  Zone* zone = compiler->zone();
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  CharacterRange::Canonicalize(non_bmp);
  for (int i = 0; i < non_bmp->length(); i++) {
    // Match surrogate pair.
    // E.g. [\u10005-\u11005] becomes
    //      \ud800[\udc05-\udfff]|
    //      [\ud801-\ud803][\udc00-\udfff]|
    //      \ud804[\udc00-\udc05]
    uc32 from = non_bmp->at(i).from();
    uc32 to = non_bmp->at(i).to();
    uc16 from_l = unibrow::Utf16::LeadSurrogate(from);
    uc16 from_t = unibrow::Utf16::TrailSurrogate(from);
    uc16 to_l = unibrow::Utf16::LeadSurrogate(to);
    uc16 to_t = unibrow::Utf16::TrailSurrogate(to);
    if (from_l == to_l) {
      // The lead surrogate is the same.
      result->AddAlternative(
          GuardedAlternative(TextNode::CreateForSurrogatePair(
              zone, CharacterRange::Singleton(from_l),
              CharacterRange::Range(from_t, to_t), compiler->read_backward(),
              on_success, default_flags)));
    } else {
      if (from_t != kTrailSurrogateStart) {
        // Add [from_l][from_t-\udfff]
        result->AddAlternative(
            GuardedAlternative(TextNode::CreateForSurrogatePair(
                zone, CharacterRange::Singleton(from_l),
                CharacterRange::Range(from_t, kTrailSurrogateEnd),
                compiler->read_backward(), on_success, default_flags)));
        from_l++;
      }
      if (to_t != kTrailSurrogateEnd) {
        // Add [to_l][\udc00-to_t]
        result->AddAlternative(
            GuardedAlternative(TextNode::CreateForSurrogatePair(
                zone, CharacterRange::Singleton(to_l),
                CharacterRange::Range(kTrailSurrogateStart, to_t),
                compiler->read_backward(), on_success, default_flags)));
        to_l--;
      }
      if (from_l <= to_l) {
        // Add [from_l-to_l][\udc00-\udfff]
        result->AddAlternative(
            GuardedAlternative(TextNode::CreateForSurrogatePair(
                zone, CharacterRange::Range(from_l, to_l),
                CharacterRange::Range(kTrailSurrogateStart, kTrailSurrogateEnd),
                compiler->read_backward(), on_success, default_flags)));
      }
    }
  }
}

RegExpNode* NegativeLookaroundAgainstReadDirectionAndMatch(
    RegExpCompiler* compiler, ZoneList<CharacterRange>* lookbehind,
    ZoneList<CharacterRange>* match, RegExpNode* on_success, bool read_backward,
    JSRegExp::Flags flags) {
  Zone* zone = compiler->zone();
  RegExpNode* match_node = TextNode::CreateForCharacterRanges(
      zone, match, read_backward, on_success, flags);
  int stack_register = compiler->UnicodeLookaroundStackRegister();
  int position_register = compiler->UnicodeLookaroundPositionRegister();
  RegExpLookaround::Builder lookaround(false, match_node, stack_register,
                                       position_register);
  RegExpNode* negative_match = TextNode::CreateForCharacterRanges(
      zone, lookbehind, !read_backward, lookaround.on_match_success(), flags);
  return lookaround.ForMatch(negative_match);
}

RegExpNode* MatchAndNegativeLookaroundInReadDirection(
    RegExpCompiler* compiler, ZoneList<CharacterRange>* match,
    ZoneList<CharacterRange>* lookahead, RegExpNode* on_success,
    bool read_backward, JSRegExp::Flags flags) {
  Zone* zone = compiler->zone();
  int stack_register = compiler->UnicodeLookaroundStackRegister();
  int position_register = compiler->UnicodeLookaroundPositionRegister();
  RegExpLookaround::Builder lookaround(false, on_success, stack_register,
                                       position_register);
  RegExpNode* negative_match = TextNode::CreateForCharacterRanges(
      zone, lookahead, read_backward, lookaround.on_match_success(), flags);
  return TextNode::CreateForCharacterRanges(
      zone, match, read_backward, lookaround.ForMatch(negative_match), flags);
}

void AddLoneLeadSurrogates(RegExpCompiler* compiler, ChoiceNode* result,
                           RegExpNode* on_success,
                           UnicodeRangeSplitter* splitter) {
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  ZoneList<CharacterRange>* lead_surrogates = splitter->lead_surrogates();
  if (lead_surrogates == nullptr) return;
  Zone* zone = compiler->zone();
  // E.g. \ud801 becomes \ud801(?![\udc00-\udfff]).
  ZoneList<CharacterRange>* trail_surrogates = CharacterRange::List(
      zone, CharacterRange::Range(kTrailSurrogateStart, kTrailSurrogateEnd));

  RegExpNode* match;
  if (compiler->read_backward()) {
    // Reading backward. Assert that reading forward, there is no trail
    // surrogate, and then backward match the lead surrogate.
    match = NegativeLookaroundAgainstReadDirectionAndMatch(
        compiler, trail_surrogates, lead_surrogates, on_success, true,
        default_flags);
  } else {
    // Reading forward. Forward match the lead surrogate and assert that
    // no trail surrogate follows.
    match = MatchAndNegativeLookaroundInReadDirection(
        compiler, lead_surrogates, trail_surrogates, on_success, false,
        default_flags);
  }
  result->AddAlternative(GuardedAlternative(match));
}

void AddLoneTrailSurrogates(RegExpCompiler* compiler, ChoiceNode* result,
                            RegExpNode* on_success,
                            UnicodeRangeSplitter* splitter) {
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  ZoneList<CharacterRange>* trail_surrogates = splitter->trail_surrogates();
  if (trail_surrogates == nullptr) return;
  Zone* zone = compiler->zone();
  // E.g. \udc01 becomes (?<![\ud800-\udbff])\udc01
  ZoneList<CharacterRange>* lead_surrogates = CharacterRange::List(
      zone, CharacterRange::Range(kLeadSurrogateStart, kLeadSurrogateEnd));

  RegExpNode* match;
  if (compiler->read_backward()) {
    // Reading backward. Backward match the trail surrogate and assert that no
    // lead surrogate precedes it.
    match = MatchAndNegativeLookaroundInReadDirection(
        compiler, trail_surrogates, lead_surrogates, on_success, true,
        default_flags);
  } else {
    // Reading forward. Assert that reading backward, there is no lead
    // surrogate, and then forward match the trail surrogate.
    match = NegativeLookaroundAgainstReadDirectionAndMatch(
        compiler, lead_surrogates, trail_surrogates, on_success, false,
        default_flags);
  }
  result->AddAlternative(GuardedAlternative(match));
}

RegExpNode* UnanchoredAdvance(RegExpCompiler* compiler,
                              RegExpNode* on_success) {
  // This implements ES2015 21.2.5.2.3, AdvanceStringIndex.
  DCHECK(!compiler->read_backward());
  Zone* zone = compiler->zone();
  // Advance any character. If the character happens to be a lead surrogate and
  // we advanced into the middle of a surrogate pair, it will work out, as
  // nothing will match from there. We will have to advance again, consuming
  // the associated trail surrogate.
  ZoneList<CharacterRange>* range = CharacterRange::List(
      zone, CharacterRange::Range(0, String::kMaxUtf16CodeUnit));
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  return TextNode::CreateForCharacterRanges(zone, range, false, on_success,
                                            default_flags);
}

void AddUnicodeCaseEquivalents(ZoneList<CharacterRange>* ranges, Zone* zone) {
#ifdef V8_INTL_SUPPORT
  DCHECK(CharacterRange::IsCanonical(ranges));

  // Micro-optimization to avoid passing large ranges to UnicodeSet::closeOver.
  // See also https://crbug.com/v8/6727.
  // TODO(jgruber): This only covers the special case of the {0,0x10FFFF} range,
  // which we use frequently internally. But large ranges can also easily be
  // created by the user. We might want to have a more general caching mechanism
  // for such ranges.
  if (ranges->length() == 1 && ranges->at(0).IsEverything(kNonBmpEnd)) return;

  // Use ICU to compute the case fold closure over the ranges.
  icu::UnicodeSet set;
  for (int i = 0; i < ranges->length(); i++) {
    set.add(ranges->at(i).from(), ranges->at(i).to());
  }
  ranges->Clear();
  set.closeOver(USET_CASE_INSENSITIVE);
  // Full case mapping map single characters to multiple characters.
  // Those are represented as strings in the set. Remove them so that
  // we end up with only simple and common case mappings.
  set.removeAllStrings();
  for (int i = 0; i < set.getRangeCount(); i++) {
    ranges->Add(CharacterRange::Range(set.getRangeStart(i), set.getRangeEnd(i)),
                zone);
  }
  // No errors and everything we collected have been ranges.
  CharacterRange::Canonicalize(ranges);
#endif  // V8_INTL_SUPPORT
}


RegExpNode* RegExpCharacterClass::ToNode(RegExpCompiler* compiler,
                                         RegExpNode* on_success) {
  set_.Canonicalize();
  Zone* zone = compiler->zone();
  ZoneList<CharacterRange>* ranges = this->ranges(zone);
  if (NeedsUnicodeCaseEquivalents(flags_)) {
    AddUnicodeCaseEquivalents(ranges, zone);
  }
  if (IsUnicode(flags_) && !compiler->one_byte() &&
      !contains_split_surrogate()) {
    if (is_negated()) {
      ZoneList<CharacterRange>* negated =
          new (zone) ZoneList<CharacterRange>(2, zone);
      CharacterRange::Negate(ranges, negated, zone);
      ranges = negated;
    }
    if (ranges->length() == 0) {
      JSRegExp::Flags default_flags = JSRegExp::Flags();
      ranges->Add(CharacterRange::Everything(), zone);
      RegExpCharacterClass* fail =
          new (zone) RegExpCharacterClass(ranges, default_flags, NEGATED);
      return new (zone) TextNode(fail, compiler->read_backward(), on_success);
    }
    if (standard_type() == '*') {
      return UnanchoredAdvance(compiler, on_success);
    } else {
      ChoiceNode* result = new (zone) ChoiceNode(2, zone);
      UnicodeRangeSplitter splitter(zone, ranges);
      AddBmpCharacters(compiler, result, on_success, &splitter);
      AddNonBmpSurrogatePairs(compiler, result, on_success, &splitter);
      AddLoneLeadSurrogates(compiler, result, on_success, &splitter);
      AddLoneTrailSurrogates(compiler, result, on_success, &splitter);
      return result;
    }
  } else {
    return new (zone) TextNode(this, compiler->read_backward(), on_success);
  }
}


int CompareFirstChar(RegExpTree* const* a, RegExpTree* const* b) {
  RegExpAtom* atom1 = (*a)->AsAtom();
  RegExpAtom* atom2 = (*b)->AsAtom();
  uc16 character1 = atom1->data().at(0);
  uc16 character2 = atom2->data().at(0);
  if (character1 < character2) return -1;
  if (character1 > character2) return 1;
  return 0;
}


static unibrow::uchar Canonical(
    unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize,
    unibrow::uchar c) {
  unibrow::uchar chars[unibrow::Ecma262Canonicalize::kMaxWidth];
  int length = canonicalize->get(c, '\0', chars);
  DCHECK_LE(length, 1);
  unibrow::uchar canonical = c;
  if (length == 1) canonical = chars[0];
  return canonical;
}


int CompareFirstCharCaseIndependent(
    unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize,
    RegExpTree* const* a, RegExpTree* const* b) {
  RegExpAtom* atom1 = (*a)->AsAtom();
  RegExpAtom* atom2 = (*b)->AsAtom();
  unibrow::uchar character1 = atom1->data().at(0);
  unibrow::uchar character2 = atom2->data().at(0);
  if (character1 == character2) return 0;
  if (character1 >= 'a' || character2 >= 'a') {
    character1 = Canonical(canonicalize, character1);
    character2 = Canonical(canonicalize, character2);
  }
  return static_cast<int>(character1) - static_cast<int>(character2);
}


// We can stable sort runs of atoms, since the order does not matter if they
// start with different characters.
// Returns true if any consecutive atoms were found.
bool RegExpDisjunction::SortConsecutiveAtoms(RegExpCompiler* compiler) {
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  int length = alternatives->length();
  bool found_consecutive_atoms = false;
  for (int i = 0; i < length; i++) {
    while (i < length) {
      RegExpTree* alternative = alternatives->at(i);
      if (alternative->IsAtom()) break;
      i++;
    }
    // i is length or it is the index of an atom.
    if (i == length) break;
    int first_atom = i;
    JSRegExp::Flags flags = alternatives->at(i)->AsAtom()->flags();
    i++;
    while (i < length) {
      RegExpTree* alternative = alternatives->at(i);
      if (!alternative->IsAtom()) break;
      if (alternative->AsAtom()->flags() != flags) break;
      i++;
    }
    // Sort atoms to get ones with common prefixes together.
    // This step is more tricky if we are in a case-independent regexp,
    // because it would change /is|I/ to /I|is/, and order matters when
    // the regexp parts don't match only disjoint starting points. To fix
    // this we have a version of CompareFirstChar that uses case-
    // independent character classes for comparison.
    DCHECK_LT(first_atom, alternatives->length());
    DCHECK_LE(i, alternatives->length());
    DCHECK_LE(first_atom, i);
    if (IgnoreCase(flags)) {
      unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize =
          compiler->isolate()->regexp_macro_assembler_canonicalize();
      auto compare_closure =
          [canonicalize](RegExpTree* const* a, RegExpTree* const* b) {
            return CompareFirstCharCaseIndependent(canonicalize, a, b);
          };
      alternatives->StableSort(compare_closure, first_atom, i - first_atom);
    } else {
      alternatives->StableSort(CompareFirstChar, first_atom, i - first_atom);
    }
    if (i - first_atom > 1) found_consecutive_atoms = true;
  }
  return found_consecutive_atoms;
}


// Optimizes ab|ac|az to a(?:b|c|d).
void RegExpDisjunction::RationalizeConsecutiveAtoms(RegExpCompiler* compiler) {
  Zone* zone = compiler->zone();
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  int length = alternatives->length();

  int write_posn = 0;
  int i = 0;
  while (i < length) {
    RegExpTree* alternative = alternatives->at(i);
    if (!alternative->IsAtom()) {
      alternatives->at(write_posn++) = alternatives->at(i);
      i++;
      continue;
    }
    RegExpAtom* const atom = alternative->AsAtom();
    JSRegExp::Flags flags = atom->flags();
    unibrow::uchar common_prefix = atom->data().at(0);
    int first_with_prefix = i;
    int prefix_length = atom->length();
    i++;
    while (i < length) {
      alternative = alternatives->at(i);
      if (!alternative->IsAtom()) break;
      RegExpAtom* const atom = alternative->AsAtom();
      if (atom->flags() != flags) break;
      unibrow::uchar new_prefix = atom->data().at(0);
      if (new_prefix != common_prefix) {
        if (!IgnoreCase(flags)) break;
        unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize =
            compiler->isolate()->regexp_macro_assembler_canonicalize();
        new_prefix = Canonical(canonicalize, new_prefix);
        common_prefix = Canonical(canonicalize, common_prefix);
        if (new_prefix != common_prefix) break;
      }
      prefix_length = Min(prefix_length, atom->length());
      i++;
    }
    if (i > first_with_prefix + 2) {
      // Found worthwhile run of alternatives with common prefix of at least one
      // character.  The sorting function above did not sort on more than one
      // character for reasons of correctness, but there may still be a longer
      // common prefix if the terms were similar or presorted in the input.
      // Find out how long the common prefix is.
      int run_length = i - first_with_prefix;
      RegExpAtom* const atom = alternatives->at(first_with_prefix)->AsAtom();
      for (int j = 1; j < run_length && prefix_length > 1; j++) {
        RegExpAtom* old_atom =
            alternatives->at(j + first_with_prefix)->AsAtom();
        for (int k = 1; k < prefix_length; k++) {
          if (atom->data().at(k) != old_atom->data().at(k)) {
            prefix_length = k;
            break;
          }
        }
      }
      RegExpAtom* prefix = new (zone)
          RegExpAtom(atom->data().SubVector(0, prefix_length), flags);
      ZoneList<RegExpTree*>* pair = new (zone) ZoneList<RegExpTree*>(2, zone);
      pair->Add(prefix, zone);
      ZoneList<RegExpTree*>* suffixes =
          new (zone) ZoneList<RegExpTree*>(run_length, zone);
      for (int j = 0; j < run_length; j++) {
        RegExpAtom* old_atom =
            alternatives->at(j + first_with_prefix)->AsAtom();
        int len = old_atom->length();
        if (len == prefix_length) {
          suffixes->Add(new (zone) RegExpEmpty(), zone);
        } else {
          RegExpTree* suffix = new (zone) RegExpAtom(
              old_atom->data().SubVector(prefix_length, old_atom->length()),
              flags);
          suffixes->Add(suffix, zone);
        }
      }
      pair->Add(new (zone) RegExpDisjunction(suffixes), zone);
      alternatives->at(write_posn++) = new (zone) RegExpAlternative(pair);
    } else {
      // Just copy any non-worthwhile alternatives.
      for (int j = first_with_prefix; j < i; j++) {
        alternatives->at(write_posn++) = alternatives->at(j);
      }
    }
  }
  alternatives->Rewind(write_posn);  // Trim end of array.
}


// Optimizes b|c|z to [bcz].
void RegExpDisjunction::FixSingleCharacterDisjunctions(
    RegExpCompiler* compiler) {
  Zone* zone = compiler->zone();
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  int length = alternatives->length();

  int write_posn = 0;
  int i = 0;
  while (i < length) {
    RegExpTree* alternative = alternatives->at(i);
    if (!alternative->IsAtom()) {
      alternatives->at(write_posn++) = alternatives->at(i);
      i++;
      continue;
    }
    RegExpAtom* const atom = alternative->AsAtom();
    if (atom->length() != 1) {
      alternatives->at(write_posn++) = alternatives->at(i);
      i++;
      continue;
    }
    JSRegExp::Flags flags = atom->flags();
    DCHECK_IMPLIES(IsUnicode(flags),
                   !unibrow::Utf16::IsLeadSurrogate(atom->data().at(0)));
    bool contains_trail_surrogate =
        unibrow::Utf16::IsTrailSurrogate(atom->data().at(0));
    int first_in_run = i;
    i++;
    // Find a run of single-character atom alternatives that have identical
    // flags (case independence and unicode-ness).
    while (i < length) {
      alternative = alternatives->at(i);
      if (!alternative->IsAtom()) break;
      RegExpAtom* const atom = alternative->AsAtom();
      if (atom->length() != 1) break;
      if (atom->flags() != flags) break;
      DCHECK_IMPLIES(IsUnicode(flags),
                     !unibrow::Utf16::IsLeadSurrogate(atom->data().at(0)));
      contains_trail_surrogate |=
          unibrow::Utf16::IsTrailSurrogate(atom->data().at(0));
      i++;
    }
    if (i > first_in_run + 1) {
      // Found non-trivial run of single-character alternatives.
      int run_length = i - first_in_run;
      ZoneList<CharacterRange>* ranges =
          new (zone) ZoneList<CharacterRange>(2, zone);
      for (int j = 0; j < run_length; j++) {
        RegExpAtom* old_atom = alternatives->at(j + first_in_run)->AsAtom();
        DCHECK_EQ(old_atom->length(), 1);
        ranges->Add(CharacterRange::Singleton(old_atom->data().at(0)), zone);
      }
      RegExpCharacterClass::CharacterClassFlags character_class_flags;
      if (IsUnicode(flags) && contains_trail_surrogate) {
        character_class_flags = RegExpCharacterClass::CONTAINS_SPLIT_SURROGATE;
      }
      alternatives->at(write_posn++) =
          new (zone) RegExpCharacterClass(ranges, flags, character_class_flags);
    } else {
      // Just copy any trivial alternatives.
      for (int j = first_in_run; j < i; j++) {
        alternatives->at(write_posn++) = alternatives->at(j);
      }
    }
  }
  alternatives->Rewind(write_posn);  // Trim end of array.
}


RegExpNode* RegExpDisjunction::ToNode(RegExpCompiler* compiler,
                                      RegExpNode* on_success) {
  ZoneList<RegExpTree*>* alternatives = this->alternatives();

  if (alternatives->length() > 2) {
    bool found_consecutive_atoms = SortConsecutiveAtoms(compiler);
    if (found_consecutive_atoms) RationalizeConsecutiveAtoms(compiler);
    FixSingleCharacterDisjunctions(compiler);
    if (alternatives->length() == 1) {
      return alternatives->at(0)->ToNode(compiler, on_success);
    }
  }

  int length = alternatives->length();

  ChoiceNode* result =
      new(compiler->zone()) ChoiceNode(length, compiler->zone());
  for (int i = 0; i < length; i++) {
    GuardedAlternative alternative(alternatives->at(i)->ToNode(compiler,
                                                               on_success));
    result->AddAlternative(alternative);
  }
  return result;
}


RegExpNode* RegExpQuantifier::ToNode(RegExpCompiler* compiler,
                                     RegExpNode* on_success) {
  return ToNode(min(),
                max(),
                is_greedy(),
                body(),
                compiler,
                on_success);
}


// Scoped object to keep track of how much we unroll quantifier loops in the
// regexp graph generator.
class RegExpExpansionLimiter {
 public:
  static const int kMaxExpansionFactor = 6;
  RegExpExpansionLimiter(RegExpCompiler* compiler, int factor)
      : compiler_(compiler),
        saved_expansion_factor_(compiler->current_expansion_factor()),
        ok_to_expand_(saved_expansion_factor_ <= kMaxExpansionFactor) {
    DCHECK_LT(0, factor);
    if (ok_to_expand_) {
      if (factor > kMaxExpansionFactor) {
        // Avoid integer overflow of the current expansion factor.
        ok_to_expand_ = false;
        compiler->set_current_expansion_factor(kMaxExpansionFactor + 1);
      } else {
        int new_factor = saved_expansion_factor_ * factor;
        ok_to_expand_ = (new_factor <= kMaxExpansionFactor);
        compiler->set_current_expansion_factor(new_factor);
      }
    }
  }

  ~RegExpExpansionLimiter() {
    compiler_->set_current_expansion_factor(saved_expansion_factor_);
  }

  bool ok_to_expand() { return ok_to_expand_; }

 private:
  RegExpCompiler* compiler_;
  int saved_expansion_factor_;
  bool ok_to_expand_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RegExpExpansionLimiter);
};


RegExpNode* RegExpQuantifier::ToNode(int min,
                                     int max,
                                     bool is_greedy,
                                     RegExpTree* body,
                                     RegExpCompiler* compiler,
                                     RegExpNode* on_success,
                                     bool not_at_start) {
  // x{f, t} becomes this:
  //
  //             (r++)<-.
  //               |     `
  //               |     (x)
  //               v     ^
  //      (r=0)-->(?)---/ [if r < t]
  //               |
  //   [if r >= f] \----> ...
  //

  // 15.10.2.5 RepeatMatcher algorithm.
  // The parser has already eliminated the case where max is 0.  In the case
  // where max_match is zero the parser has removed the quantifier if min was
  // > 0 and removed the atom if min was 0.  See AddQuantifierToAtom.

  // If we know that we cannot match zero length then things are a little
  // simpler since we don't need to make the special zero length match check
  // from step 2.1.  If the min and max are small we can unroll a little in
  // this case.
  static const int kMaxUnrolledMinMatches = 3;  // Unroll (foo)+ and (foo){3,}
  static const int kMaxUnrolledMaxMatches = 3;  // Unroll (foo)? and (foo){x,3}
  if (max == 0) return on_success;  // This can happen due to recursion.
  bool body_can_be_empty = (body->min_match() == 0);
  int body_start_reg = RegExpCompiler::kNoRegister;
  Interval capture_registers = body->CaptureRegisters();
  bool needs_capture_clearing = !capture_registers.is_empty();
  Zone* zone = compiler->zone();

  if (body_can_be_empty) {
    body_start_reg = compiler->AllocateRegister();
  } else if (compiler->optimize() && !needs_capture_clearing) {
    // Only unroll if there are no captures and the body can't be
    // empty.
    {
      RegExpExpansionLimiter limiter(
          compiler, min + ((max != min) ? 1 : 0));
      if (min > 0 && min <= kMaxUnrolledMinMatches && limiter.ok_to_expand()) {
        int new_max = (max == kInfinity) ? max : max - min;
        // Recurse once to get the loop or optional matches after the fixed
        // ones.
        RegExpNode* answer = ToNode(
            0, new_max, is_greedy, body, compiler, on_success, true);
        // Unroll the forced matches from 0 to min.  This can cause chains of
        // TextNodes (which the parser does not generate).  These should be
        // combined if it turns out they hinder good code generation.
        for (int i = 0; i < min; i++) {
          answer = body->ToNode(compiler, answer);
        }
        return answer;
      }
    }
    if (max <= kMaxUnrolledMaxMatches && min == 0) {
      DCHECK_LT(0, max);  // Due to the 'if' above.
      RegExpExpansionLimiter limiter(compiler, max);
      if (limiter.ok_to_expand()) {
        // Unroll the optional matches up to max.
        RegExpNode* answer = on_success;
        for (int i = 0; i < max; i++) {
          ChoiceNode* alternation = new(zone) ChoiceNode(2, zone);
          if (is_greedy) {
            alternation->AddAlternative(
                GuardedAlternative(body->ToNode(compiler, answer)));
            alternation->AddAlternative(GuardedAlternative(on_success));
          } else {
            alternation->AddAlternative(GuardedAlternative(on_success));
            alternation->AddAlternative(
                GuardedAlternative(body->ToNode(compiler, answer)));
          }
          answer = alternation;
          if (not_at_start && !compiler->read_backward()) {
            alternation->set_not_at_start();
          }
        }
        return answer;
      }
    }
  }
  bool has_min = min > 0;
  bool has_max = max < RegExpTree::kInfinity;
  bool needs_counter = has_min || has_max;
  int reg_ctr = needs_counter
      ? compiler->AllocateRegister()
      : RegExpCompiler::kNoRegister;
  LoopChoiceNode* center = new (zone)
      LoopChoiceNode(body->min_match() == 0, compiler->read_backward(), zone);
  if (not_at_start && !compiler->read_backward()) center->set_not_at_start();
  RegExpNode* loop_return = needs_counter
      ? static_cast<RegExpNode*>(ActionNode::IncrementRegister(reg_ctr, center))
      : static_cast<RegExpNode*>(center);
  if (body_can_be_empty) {
    // If the body can be empty we need to check if it was and then
    // backtrack.
    loop_return = ActionNode::EmptyMatchCheck(body_start_reg,
                                              reg_ctr,
                                              min,
                                              loop_return);
  }
  RegExpNode* body_node = body->ToNode(compiler, loop_return);
  if (body_can_be_empty) {
    // If the body can be empty we need to store the start position
    // so we can bail out if it was empty.
    body_node = ActionNode::StorePosition(body_start_reg, false, body_node);
  }
  if (needs_capture_clearing) {
    // Before entering the body of this loop we need to clear captures.
    body_node = ActionNode::ClearCaptures(capture_registers, body_node);
  }
  GuardedAlternative body_alt(body_node);
  if (has_max) {
    Guard* body_guard =
        new(zone) Guard(reg_ctr, Guard::LT, max);
    body_alt.AddGuard(body_guard, zone);
  }
  GuardedAlternative rest_alt(on_success);
  if (has_min) {
    Guard* rest_guard = new(compiler->zone()) Guard(reg_ctr, Guard::GEQ, min);
    rest_alt.AddGuard(rest_guard, zone);
  }
  if (is_greedy) {
    center->AddLoopAlternative(body_alt);
    center->AddContinueAlternative(rest_alt);
  } else {
    center->AddContinueAlternative(rest_alt);
    center->AddLoopAlternative(body_alt);
  }
  if (needs_counter) {
    return ActionNode::SetRegister(reg_ctr, 0, center);
  } else {
    return center;
  }
}

namespace {
// Desugar \b to (?<=\w)(?=\W)|(?<=\W)(?=\w) and
//         \B to (?<=\w)(?=\w)|(?<=\W)(?=\W)
RegExpNode* BoundaryAssertionAsLookaround(RegExpCompiler* compiler,
                                          RegExpNode* on_success,
                                          RegExpAssertion::AssertionType type,
                                          JSRegExp::Flags flags) {
  DCHECK(NeedsUnicodeCaseEquivalents(flags));
  Zone* zone = compiler->zone();
  ZoneList<CharacterRange>* word_range =
      new (zone) ZoneList<CharacterRange>(2, zone);
  CharacterRange::AddClassEscape('w', word_range, true, zone);
  int stack_register = compiler->UnicodeLookaroundStackRegister();
  int position_register = compiler->UnicodeLookaroundPositionRegister();
  ChoiceNode* result = new (zone) ChoiceNode(2, zone);
  // Add two choices. The (non-)boundary could start with a word or
  // a non-word-character.
  for (int i = 0; i < 2; i++) {
    bool lookbehind_for_word = i == 0;
    bool lookahead_for_word =
        (type == RegExpAssertion::BOUNDARY) ^ lookbehind_for_word;
    // Look to the left.
    RegExpLookaround::Builder lookbehind(lookbehind_for_word, on_success,
                                         stack_register, position_register);
    RegExpNode* backward = TextNode::CreateForCharacterRanges(
        zone, word_range, true, lookbehind.on_match_success(), flags);
    // Look to the right.
    RegExpLookaround::Builder lookahead(lookahead_for_word,
                                        lookbehind.ForMatch(backward),
                                        stack_register, position_register);
    RegExpNode* forward = TextNode::CreateForCharacterRanges(
        zone, word_range, false, lookahead.on_match_success(), flags);
    result->AddAlternative(GuardedAlternative(lookahead.ForMatch(forward)));
  }
  return result;
}
}  // anonymous namespace

RegExpNode* RegExpAssertion::ToNode(RegExpCompiler* compiler,
                                    RegExpNode* on_success) {
  NodeInfo info;
  Zone* zone = compiler->zone();

  switch (assertion_type()) {
    case START_OF_LINE:
      return AssertionNode::AfterNewline(on_success);
    case START_OF_INPUT:
      return AssertionNode::AtStart(on_success);
    case BOUNDARY:
      return NeedsUnicodeCaseEquivalents(flags_)
                 ? BoundaryAssertionAsLookaround(compiler, on_success, BOUNDARY,
                                                 flags_)
                 : AssertionNode::AtBoundary(on_success);
    case NON_BOUNDARY:
      return NeedsUnicodeCaseEquivalents(flags_)
                 ? BoundaryAssertionAsLookaround(compiler, on_success,
                                                 NON_BOUNDARY, flags_)
                 : AssertionNode::AtNonBoundary(on_success);
    case END_OF_INPUT:
      return AssertionNode::AtEnd(on_success);
    case END_OF_LINE: {
      // Compile $ in multiline regexps as an alternation with a positive
      // lookahead in one side and an end-of-input on the other side.
      // We need two registers for the lookahead.
      int stack_pointer_register = compiler->AllocateRegister();
      int position_register = compiler->AllocateRegister();
      // The ChoiceNode to distinguish between a newline and end-of-input.
      ChoiceNode* result = new(zone) ChoiceNode(2, zone);
      // Create a newline atom.
      ZoneList<CharacterRange>* newline_ranges =
          new(zone) ZoneList<CharacterRange>(3, zone);
      CharacterRange::AddClassEscape('n', newline_ranges, false, zone);
      JSRegExp::Flags default_flags = JSRegExp::Flags();
      RegExpCharacterClass* newline_atom =
          new (zone) RegExpCharacterClass('n', default_flags);
      TextNode* newline_matcher = new (zone) TextNode(
          newline_atom, false, ActionNode::PositiveSubmatchSuccess(
                                   stack_pointer_register, position_register,
                                   0,   // No captures inside.
                                   -1,  // Ignored if no captures.
                                   on_success));
      // Create an end-of-input matcher.
      RegExpNode* end_of_line = ActionNode::BeginSubmatch(
          stack_pointer_register,
          position_register,
          newline_matcher);
      // Add the two alternatives to the ChoiceNode.
      GuardedAlternative eol_alternative(end_of_line);
      result->AddAlternative(eol_alternative);
      GuardedAlternative end_alternative(AssertionNode::AtEnd(on_success));
      result->AddAlternative(end_alternative);
      return result;
    }
    default:
      UNREACHABLE();
  }
  return on_success;
}


RegExpNode* RegExpBackReference::ToNode(RegExpCompiler* compiler,
                                        RegExpNode* on_success) {
  return new (compiler->zone())
      BackReferenceNode(RegExpCapture::StartRegister(index()),
                        RegExpCapture::EndRegister(index()), flags_,
                        compiler->read_backward(), on_success);
}


RegExpNode* RegExpEmpty::ToNode(RegExpCompiler* compiler,
                                RegExpNode* on_success) {
  return on_success;
}


RegExpLookaround::Builder::Builder(bool is_positive, RegExpNode* on_success,
                                   int stack_pointer_register,
                                   int position_register,
                                   int capture_register_count,
                                   int capture_register_start)
    : is_positive_(is_positive),
      on_success_(on_success),
      stack_pointer_register_(stack_pointer_register),
      position_register_(position_register) {
  if (is_positive_) {
    on_match_success_ = ActionNode::PositiveSubmatchSuccess(
        stack_pointer_register, position_register, capture_register_count,
        capture_register_start, on_success_);
  } else {
    Zone* zone = on_success_->zone();
    on_match_success_ = new (zone) NegativeSubmatchSuccess(
        stack_pointer_register, position_register, capture_register_count,
        capture_register_start, zone);
  }
}


RegExpNode* RegExpLookaround::Builder::ForMatch(RegExpNode* match) {
  if (is_positive_) {
    return ActionNode::BeginSubmatch(stack_pointer_register_,
                                     position_register_, match);
  } else {
    Zone* zone = on_success_->zone();
    // We use a ChoiceNode to represent the negative lookaround. The first
    // alternative is the negative match. On success, the end node backtracks.
    // On failure, the second alternative is tried and leads to success.
    // NegativeLookaheadChoiceNode is a special ChoiceNode that ignores the
    // first exit when calculating quick checks.
    ChoiceNode* choice_node = new (zone) NegativeLookaroundChoiceNode(
        GuardedAlternative(match), GuardedAlternative(on_success_), zone);
    return ActionNode::BeginSubmatch(stack_pointer_register_,
                                     position_register_, choice_node);
  }
}


RegExpNode* RegExpLookaround::ToNode(RegExpCompiler* compiler,
                                     RegExpNode* on_success) {
  int stack_pointer_register = compiler->AllocateRegister();
  int position_register = compiler->AllocateRegister();

  const int registers_per_capture = 2;
  const int register_of_first_capture = 2;
  int register_count = capture_count_ * registers_per_capture;
  int register_start =
    register_of_first_capture + capture_from_ * registers_per_capture;

  RegExpNode* result;
  bool was_reading_backward = compiler->read_backward();
  compiler->set_read_backward(type() == LOOKBEHIND);
  Builder builder(is_positive(), on_success, stack_pointer_register,
                  position_register, register_count, register_start);
  RegExpNode* match = body_->ToNode(compiler, builder.on_match_success());
  result = builder.ForMatch(match);
  compiler->set_read_backward(was_reading_backward);
  return result;
}


RegExpNode* RegExpCapture::ToNode(RegExpCompiler* compiler,
                                  RegExpNode* on_success) {
  return ToNode(body(), index(), compiler, on_success);
}


RegExpNode* RegExpCapture::ToNode(RegExpTree* body,
                                  int index,
                                  RegExpCompiler* compiler,
                                  RegExpNode* on_success) {
  DCHECK_NOT_NULL(body);
  int start_reg = RegExpCapture::StartRegister(index);
  int end_reg = RegExpCapture::EndRegister(index);
  if (compiler->read_backward()) std::swap(start_reg, end_reg);
  RegExpNode* store_end = ActionNode::StorePosition(end_reg, true, on_success);
  RegExpNode* body_node = body->ToNode(compiler, store_end);
  return ActionNode::StorePosition(start_reg, true, body_node);
}


RegExpNode* RegExpAlternative::ToNode(RegExpCompiler* compiler,
                                      RegExpNode* on_success) {
  ZoneList<RegExpTree*>* children = nodes();
  RegExpNode* current = on_success;
  if (compiler->read_backward()) {
    for (int i = 0; i < children->length(); i++) {
      current = children->at(i)->ToNode(compiler, current);
    }
  } else {
    for (int i = children->length() - 1; i >= 0; i--) {
      current = children->at(i)->ToNode(compiler, current);
    }
  }
  return current;
}


static void AddClass(const int* elmv,
                     int elmc,
                     ZoneList<CharacterRange>* ranges,
                     Zone* zone) {
  elmc--;
  DCHECK_EQ(kRangeEndMarker, elmv[elmc]);
  for (int i = 0; i < elmc; i += 2) {
    DCHECK(elmv[i] < elmv[i + 1]);
    ranges->Add(CharacterRange::Range(elmv[i], elmv[i + 1] - 1), zone);
  }
}


static void AddClassNegated(const int *elmv,
                            int elmc,
                            ZoneList<CharacterRange>* ranges,
                            Zone* zone) {
  elmc--;
  DCHECK_EQ(kRangeEndMarker, elmv[elmc]);
  DCHECK_NE(0x0000, elmv[0]);
  DCHECK_NE(String::kMaxCodePoint, elmv[elmc - 1]);
  uc16 last = 0x0000;
  for (int i = 0; i < elmc; i += 2) {
    DCHECK(last <= elmv[i] - 1);
    DCHECK(elmv[i] < elmv[i + 1]);
    ranges->Add(CharacterRange::Range(last, elmv[i] - 1), zone);
    last = elmv[i + 1];
  }
  ranges->Add(CharacterRange::Range(last, String::kMaxCodePoint), zone);
}

void CharacterRange::AddClassEscape(char type, ZoneList<CharacterRange>* ranges,
                                    bool add_unicode_case_equivalents,
                                    Zone* zone) {
  if (add_unicode_case_equivalents && (type == 'w' || type == 'W')) {
    // See #sec-runtime-semantics-wordcharacters-abstract-operation
    // In case of unicode and ignore_case, we need to create the closure over
    // case equivalent characters before negating.
    ZoneList<CharacterRange>* new_ranges =
        new (zone) ZoneList<CharacterRange>(2, zone);
    AddClass(kWordRanges, kWordRangeCount, new_ranges, zone);
    AddUnicodeCaseEquivalents(new_ranges, zone);
    if (type == 'W') {
      ZoneList<CharacterRange>* negated =
          new (zone) ZoneList<CharacterRange>(2, zone);
      CharacterRange::Negate(new_ranges, negated, zone);
      new_ranges = negated;
    }
    ranges->AddAll(*new_ranges, zone);
    return;
  }
  AddClassEscape(type, ranges, zone);
}

void CharacterRange::AddClassEscape(char type, ZoneList<CharacterRange>* ranges,
                                    Zone* zone) {
  switch (type) {
    case 's':
      AddClass(kSpaceRanges, kSpaceRangeCount, ranges, zone);
      break;
    case 'S':
      AddClassNegated(kSpaceRanges, kSpaceRangeCount, ranges, zone);
      break;
    case 'w':
      AddClass(kWordRanges, kWordRangeCount, ranges, zone);
      break;
    case 'W':
      AddClassNegated(kWordRanges, kWordRangeCount, ranges, zone);
      break;
    case 'd':
      AddClass(kDigitRanges, kDigitRangeCount, ranges, zone);
      break;
    case 'D':
      AddClassNegated(kDigitRanges, kDigitRangeCount, ranges, zone);
      break;
    case '.':
      AddClassNegated(kLineTerminatorRanges,
                      kLineTerminatorRangeCount,
                      ranges,
                      zone);
      break;
    // This is not a character range as defined by the spec but a
    // convenient shorthand for a character class that matches any
    // character.
    case '*':
      ranges->Add(CharacterRange::Everything(), zone);
      break;
    // This is the set of characters matched by the $ and ^ symbols
    // in multiline mode.
    case 'n':
      AddClass(kLineTerminatorRanges,
               kLineTerminatorRangeCount,
               ranges,
               zone);
      break;
    default:
      UNREACHABLE();
  }
}


Vector<const int> CharacterRange::GetWordBounds() {
  return Vector<const int>(kWordRanges, kWordRangeCount - 1);
}

// static
void CharacterRange::AddCaseEquivalents(Isolate* isolate, Zone* zone,
                                        ZoneList<CharacterRange>* ranges,
                                        bool is_one_byte) {
  CharacterRange::Canonicalize(ranges);
  int range_count = ranges->length();
  for (int i = 0; i < range_count; i++) {
    CharacterRange range = ranges->at(i);
    uc32 bottom = range.from();
    if (bottom > String::kMaxUtf16CodeUnit) continue;
    uc32 top = Min(range.to(), String::kMaxUtf16CodeUnit);
    // Nothing to be done for surrogates.
    if (bottom >= kLeadSurrogateStart && top <= kTrailSurrogateEnd) continue;
    if (is_one_byte && !RangeContainsLatin1Equivalents(range)) {
      if (bottom > String::kMaxOneByteCharCode) continue;
      if (top > String::kMaxOneByteCharCode) top = String::kMaxOneByteCharCode;
    }
    unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
    if (top == bottom) {
      // If this is a singleton we just expand the one character.
      int length = isolate->jsregexp_uncanonicalize()->get(bottom, '\0', chars);
      for (int i = 0; i < length; i++) {
        uc32 chr = chars[i];
        if (chr != bottom) {
          ranges->Add(CharacterRange::Singleton(chars[i]), zone);
        }
      }
    } else {
      // If this is a range we expand the characters block by block, expanding
      // contiguous subranges (blocks) one at a time.  The approach is as
      // follows.  For a given start character we look up the remainder of the
      // block that contains it (represented by the end point), for instance we
      // find 'z' if the character is 'c'.  A block is characterized by the
      // property that all characters uncanonicalize in the same way, except
      // that each entry in the result is incremented by the distance from the
      // first element.  So a-z is a block because 'a' uncanonicalizes to ['a',
      // 'A'] and the k'th letter uncanonicalizes to ['a' + k, 'A' + k].  Once
      // we've found the end point we look up its uncanonicalization and
      // produce a range for each element.  For instance for [c-f] we look up
      // ['z', 'Z'] and produce [c-f] and [C-F].  We then only add a range if
      // it is not already contained in the input, so [c-f] will be skipped but
      // [C-F] will be added.  If this range is not completely contained in a
      // block we do this for all the blocks covered by the range (handling
      // characters that is not in a block as a "singleton block").
      unibrow::uchar equivalents[unibrow::Ecma262UnCanonicalize::kMaxWidth];
      int pos = bottom;
      while (pos <= top) {
        int length =
            isolate->jsregexp_canonrange()->get(pos, '\0', equivalents);
        uc32 block_end;
        if (length == 0) {
          block_end = pos;
        } else {
          DCHECK_EQ(1, length);
          block_end = equivalents[0];
        }
        int end = (block_end > top) ? top : block_end;
        length = isolate->jsregexp_uncanonicalize()->get(block_end, '\0',
                                                         equivalents);
        for (int i = 0; i < length; i++) {
          uc32 c = equivalents[i];
          uc32 range_from = c - (block_end - pos);
          uc32 range_to = c - (block_end - end);
          if (!(bottom <= range_from && range_to <= top)) {
            ranges->Add(CharacterRange::Range(range_from, range_to), zone);
          }
        }
        pos = end + 1;
      }
    }
  }
}


bool CharacterRange::IsCanonical(ZoneList<CharacterRange>* ranges) {
  DCHECK_NOT_NULL(ranges);
  int n = ranges->length();
  if (n <= 1) return true;
  int max = ranges->at(0).to();
  for (int i = 1; i < n; i++) {
    CharacterRange next_range = ranges->at(i);
    if (next_range.from() <= max + 1) return false;
    max = next_range.to();
  }
  return true;
}


ZoneList<CharacterRange>* CharacterSet::ranges(Zone* zone) {
  if (ranges_ == nullptr) {
    ranges_ = new(zone) ZoneList<CharacterRange>(2, zone);
    CharacterRange::AddClassEscape(standard_set_type_, ranges_, false, zone);
  }
  return ranges_;
}


// Move a number of elements in a zonelist to another position
// in the same list. Handles overlapping source and target areas.
static void MoveRanges(ZoneList<CharacterRange>* list,
                       int from,
                       int to,
                       int count) {
  // Ranges are potentially overlapping.
  if (from < to) {
    for (int i = count - 1; i >= 0; i--) {
      list->at(to + i) = list->at(from + i);
    }
  } else {
    for (int i = 0; i < count; i++) {
      list->at(to + i) = list->at(from + i);
    }
  }
}


static int InsertRangeInCanonicalList(ZoneList<CharacterRange>* list,
                                      int count,
                                      CharacterRange insert) {
  // Inserts a range into list[0..count[, which must be sorted
  // by from value and non-overlapping and non-adjacent, using at most
  // list[0..count] for the result. Returns the number of resulting
  // canonicalized ranges. Inserting a range may collapse existing ranges into
  // fewer ranges, so the return value can be anything in the range 1..count+1.
  uc32 from = insert.from();
  uc32 to = insert.to();
  int start_pos = 0;
  int end_pos = count;
  for (int i = count - 1; i >= 0; i--) {
    CharacterRange current = list->at(i);
    if (current.from() > to + 1) {
      end_pos = i;
    } else if (current.to() + 1 < from) {
      start_pos = i + 1;
      break;
    }
  }

  // Inserted range overlaps, or is adjacent to, ranges at positions
  // [start_pos..end_pos[. Ranges before start_pos or at or after end_pos are
  // not affected by the insertion.
  // If start_pos == end_pos, the range must be inserted before start_pos.
  // if start_pos < end_pos, the entire range from start_pos to end_pos
  // must be merged with the insert range.

  if (start_pos == end_pos) {
    // Insert between existing ranges at position start_pos.
    if (start_pos < count) {
      MoveRanges(list, start_pos, start_pos + 1, count - start_pos);
    }
    list->at(start_pos) = insert;
    return count + 1;
  }
  if (start_pos + 1 == end_pos) {
    // Replace single existing range at position start_pos.
    CharacterRange to_replace = list->at(start_pos);
    int new_from = Min(to_replace.from(), from);
    int new_to = Max(to_replace.to(), to);
    list->at(start_pos) = CharacterRange::Range(new_from, new_to);
    return count;
  }
  // Replace a number of existing ranges from start_pos to end_pos - 1.
  // Move the remaining ranges down.

  int new_from = Min(list->at(start_pos).from(), from);
  int new_to = Max(list->at(end_pos - 1).to(), to);
  if (end_pos < count) {
    MoveRanges(list, end_pos, start_pos + 1, count - end_pos);
  }
  list->at(start_pos) = CharacterRange::Range(new_from, new_to);
  return count - (end_pos - start_pos) + 1;
}


void CharacterSet::Canonicalize() {
  // Special/default classes are always considered canonical. The result
  // of calling ranges() will be sorted.
  if (ranges_ == nullptr) return;
  CharacterRange::Canonicalize(ranges_);
}


void CharacterRange::Canonicalize(ZoneList<CharacterRange>* character_ranges) {
  if (character_ranges->length() <= 1) return;
  // Check whether ranges are already canonical (increasing, non-overlapping,
  // non-adjacent).
  int n = character_ranges->length();
  int max = character_ranges->at(0).to();
  int i = 1;
  while (i < n) {
    CharacterRange current = character_ranges->at(i);
    if (current.from() <= max + 1) {
      break;
    }
    max = current.to();
    i++;
  }
  // Canonical until the i'th range. If that's all of them, we are done.
  if (i == n) return;

  // The ranges at index i and forward are not canonicalized. Make them so by
  // doing the equivalent of insertion sort (inserting each into the previous
  // list, in order).
  // Notice that inserting a range can reduce the number of ranges in the
  // result due to combining of adjacent and overlapping ranges.
  int read = i;  // Range to insert.
  int num_canonical = i;  // Length of canonicalized part of list.
  do {
    num_canonical = InsertRangeInCanonicalList(character_ranges,
                                               num_canonical,
                                               character_ranges->at(read));
    read++;
  } while (read < n);
  character_ranges->Rewind(num_canonical);

  DCHECK(CharacterRange::IsCanonical(character_ranges));
}


void CharacterRange::Negate(ZoneList<CharacterRange>* ranges,
                            ZoneList<CharacterRange>* negated_ranges,
                            Zone* zone) {
  DCHECK(CharacterRange::IsCanonical(ranges));
  DCHECK_EQ(0, negated_ranges->length());
  int range_count = ranges->length();
  uc32 from = 0;
  int i = 0;
  if (range_count > 0 && ranges->at(0).from() == 0) {
    from = ranges->at(0).to() + 1;
    i = 1;
  }
  while (i < range_count) {
    CharacterRange range = ranges->at(i);
    negated_ranges->Add(CharacterRange::Range(from, range.from() - 1), zone);
    from = range.to() + 1;
    i++;
  }
  if (from < String::kMaxCodePoint) {
    negated_ranges->Add(CharacterRange::Range(from, String::kMaxCodePoint),
                        zone);
  }
}


// -------------------------------------------------------------------
// Splay tree


OutSet* OutSet::Extend(unsigned value, Zone* zone) {
  if (Get(value))
    return this;
  if (successors(zone) != nullptr) {
    for (int i = 0; i < successors(zone)->length(); i++) {
      OutSet* successor = successors(zone)->at(i);
      if (successor->Get(value))
        return successor;
    }
  } else {
    successors_ = new(zone) ZoneList<OutSet*>(2, zone);
  }
  OutSet* result = new(zone) OutSet(first_, remaining_);
  result->Set(value, zone);
  successors(zone)->Add(result, zone);
  return result;
}


void OutSet::Set(unsigned value, Zone *zone) {
  if (value < kFirstLimit) {
    first_ |= (1 << value);
  } else {
    if (remaining_ == nullptr)
      remaining_ = new(zone) ZoneList<unsigned>(1, zone);
    if (remaining_->is_empty() || !remaining_->Contains(value))
      remaining_->Add(value, zone);
  }
}


bool OutSet::Get(unsigned value) const {
  if (value < kFirstLimit) {
    return (first_ & (1 << value)) != 0;
  } else if (remaining_ == nullptr) {
    return false;
  } else {
    return remaining_->Contains(value);
  }
}


const uc32 DispatchTable::Config::kNoKey = unibrow::Utf8::kBadChar;


void DispatchTable::AddRange(CharacterRange full_range, int value,
                             Zone* zone) {
  CharacterRange current = full_range;
  if (tree()->is_empty()) {
    // If this is the first range we just insert into the table.
    ZoneSplayTree<Config>::Locator loc;
    bool inserted = tree()->Insert(current.from(), &loc);
    DCHECK(inserted);
    USE(inserted);
    loc.set_value(Entry(current.from(), current.to(),
                        empty()->Extend(value, zone)));
    return;
  }
  // First see if there is a range to the left of this one that
  // overlaps.
  ZoneSplayTree<Config>::Locator loc;
  if (tree()->FindGreatestLessThan(current.from(), &loc)) {
    Entry* entry = &loc.value();
    // If we've found a range that overlaps with this one, and it
    // starts strictly to the left of this one, we have to fix it
    // because the following code only handles ranges that start on
    // or after the start point of the range we're adding.
    if (entry->from() < current.from() && entry->to() >= current.from()) {
      // Snap the overlapping range in half around the start point of
      // the range we're adding.
      CharacterRange left =
          CharacterRange::Range(entry->from(), current.from() - 1);
      CharacterRange right = CharacterRange::Range(current.from(), entry->to());
      // The left part of the overlapping range doesn't overlap.
      // Truncate the whole entry to be just the left part.
      entry->set_to(left.to());
      // The right part is the one that overlaps.  We add this part
      // to the map and let the next step deal with merging it with
      // the range we're adding.
      ZoneSplayTree<Config>::Locator loc;
      bool inserted = tree()->Insert(right.from(), &loc);
      DCHECK(inserted);
      USE(inserted);
      loc.set_value(Entry(right.from(),
                          right.to(),
                          entry->out_set()));
    }
  }
  while (current.is_valid()) {
    if (tree()->FindLeastGreaterThan(current.from(), &loc) &&
        (loc.value().from() <= current.to()) &&
        (loc.value().to() >= current.from())) {
      Entry* entry = &loc.value();
      // We have overlap.  If there is space between the start point of
      // the range we're adding and where the overlapping range starts
      // then we have to add a range covering just that space.
      if (current.from() < entry->from()) {
        ZoneSplayTree<Config>::Locator ins;
        bool inserted = tree()->Insert(current.from(), &ins);
        DCHECK(inserted);
        USE(inserted);
        ins.set_value(Entry(current.from(),
                            entry->from() - 1,
                            empty()->Extend(value, zone)));
        current.set_from(entry->from());
      }
      DCHECK_EQ(current.from(), entry->from());
      // If the overlapping range extends beyond the one we want to add
      // we have to snap the right part off and add it separately.
      if (entry->to() > current.to()) {
        ZoneSplayTree<Config>::Locator ins;
        bool inserted = tree()->Insert(current.to() + 1, &ins);
        DCHECK(inserted);
        USE(inserted);
        ins.set_value(Entry(current.to() + 1,
                            entry->to(),
                            entry->out_set()));
        entry->set_to(current.to());
      }
      DCHECK(entry->to() <= current.to());
      // The overlapping range is now completely contained by the range
      // we're adding so we can just update it and move the start point
      // of the range we're adding just past it.
      entry->AddValue(value, zone);
      DCHECK(entry->to() + 1 > current.from());
      current.set_from(entry->to() + 1);
    } else {
      // There is no overlap so we can just add the range
      ZoneSplayTree<Config>::Locator ins;
      bool inserted = tree()->Insert(current.from(), &ins);
      DCHECK(inserted);
      USE(inserted);
      ins.set_value(Entry(current.from(),
                          current.to(),
                          empty()->Extend(value, zone)));
      break;
    }
  }
}


OutSet* DispatchTable::Get(uc32 value) {
  ZoneSplayTree<Config>::Locator loc;
  if (!tree()->FindGreatestLessThan(value, &loc))
    return empty();
  Entry* entry = &loc.value();
  if (value <= entry->to())
    return entry->out_set();
  else
    return empty();
}


// -------------------------------------------------------------------
// Analysis


void Analysis::EnsureAnalyzed(RegExpNode* that) {
  StackLimitCheck check(isolate());
  if (check.HasOverflowed()) {
    fail("Stack overflow");
    return;
  }
  if (that->info()->been_analyzed || that->info()->being_analyzed)
    return;
  that->info()->being_analyzed = true;
  that->Accept(this);
  that->info()->being_analyzed = false;
  that->info()->been_analyzed = true;
}


void Analysis::VisitEnd(EndNode* that) {
  // nothing to do
}


void TextNode::CalculateOffsets() {
  int element_count = elements()->length();
  // Set up the offsets of the elements relative to the start.  This is a fixed
  // quantity since a TextNode can only contain fixed-width things.
  int cp_offset = 0;
  for (int i = 0; i < element_count; i++) {
    TextElement& elm = elements()->at(i);
    elm.set_cp_offset(cp_offset);
    cp_offset += elm.length();
  }
}


void Analysis::VisitText(TextNode* that) {
  that->MakeCaseIndependent(isolate(), is_one_byte_);
  EnsureAnalyzed(that->on_success());
  if (!has_failed()) {
    that->CalculateOffsets();
  }
}


void Analysis::VisitAction(ActionNode* that) {
  RegExpNode* target = that->on_success();
  EnsureAnalyzed(target);
  if (!has_failed()) {
    // If the next node is interested in what it follows then this node
    // has to be interested too so it can pass the information on.
    that->info()->AddFromFollowing(target->info());
  }
}


void Analysis::VisitChoice(ChoiceNode* that) {
  NodeInfo* info = that->info();
  for (int i = 0; i < that->alternatives()->length(); i++) {
    RegExpNode* node = that->alternatives()->at(i).node();
    EnsureAnalyzed(node);
    if (has_failed()) return;
    // Anything the following nodes need to know has to be known by
    // this node also, so it can pass it on.
    info->AddFromFollowing(node->info());
  }
}


void Analysis::VisitLoopChoice(LoopChoiceNode* that) {
  NodeInfo* info = that->info();
  for (int i = 0; i < that->alternatives()->length(); i++) {
    RegExpNode* node = that->alternatives()->at(i).node();
    if (node != that->loop_node()) {
      EnsureAnalyzed(node);
      if (has_failed()) return;
      info->AddFromFollowing(node->info());
    }
  }
  // Check the loop last since it may need the value of this node
  // to get a correct result.
  EnsureAnalyzed(that->loop_node());
  if (!has_failed()) {
    info->AddFromFollowing(that->loop_node()->info());
  }
}


void Analysis::VisitBackReference(BackReferenceNode* that) {
  EnsureAnalyzed(that->on_success());
}


void Analysis::VisitAssertion(AssertionNode* that) {
  EnsureAnalyzed(that->on_success());
}


void BackReferenceNode::FillInBMInfo(Isolate* isolate, int offset, int budget,
                                     BoyerMooreLookahead* bm,
                                     bool not_at_start) {
  // Working out the set of characters that a backreference can match is too
  // hard, so we just say that any character can match.
  bm->SetRest(offset);
  SaveBMInfo(bm, not_at_start, offset);
}


STATIC_ASSERT(BoyerMoorePositionInfo::kMapSize ==
              RegExpMacroAssembler::kTableSize);


void ChoiceNode::FillInBMInfo(Isolate* isolate, int offset, int budget,
                              BoyerMooreLookahead* bm, bool not_at_start) {
  ZoneList<GuardedAlternative>* alts = alternatives();
  budget = (budget - 1) / alts->length();
  for (int i = 0; i < alts->length(); i++) {
    GuardedAlternative& alt = alts->at(i);
    if (alt.guards() != nullptr && alt.guards()->length() != 0) {
      bm->SetRest(offset);  // Give up trying to fill in info.
      SaveBMInfo(bm, not_at_start, offset);
      return;
    }
    alt.node()->FillInBMInfo(isolate, offset, budget, bm, not_at_start);
  }
  SaveBMInfo(bm, not_at_start, offset);
}


void TextNode::FillInBMInfo(Isolate* isolate, int initial_offset, int budget,
                            BoyerMooreLookahead* bm, bool not_at_start) {
  if (initial_offset >= bm->length()) return;
  int offset = initial_offset;
  int max_char = bm->max_char();
  for (int i = 0; i < elements()->length(); i++) {
    if (offset >= bm->length()) {
      if (initial_offset == 0) set_bm_info(not_at_start, bm);
      return;
    }
    TextElement text = elements()->at(i);
    if (text.text_type() == TextElement::ATOM) {
      RegExpAtom* atom = text.atom();
      for (int j = 0; j < atom->length(); j++, offset++) {
        if (offset >= bm->length()) {
          if (initial_offset == 0) set_bm_info(not_at_start, bm);
          return;
        }
        uc16 character = atom->data()[j];
        if (IgnoreCase(atom->flags())) {
          unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
          int length = GetCaseIndependentLetters(
              isolate, character, bm->max_char() == String::kMaxOneByteCharCode,
              chars);
          for (int j = 0; j < length; j++) {
            bm->Set(offset, chars[j]);
          }
        } else {
          if (character <= max_char) bm->Set(offset, character);
        }
      }
    } else {
      DCHECK_EQ(TextElement::CHAR_CLASS, text.text_type());
      RegExpCharacterClass* char_class = text.char_class();
      ZoneList<CharacterRange>* ranges = char_class->ranges(zone());
      if (char_class->is_negated()) {
        bm->SetAll(offset);
      } else {
        for (int k = 0; k < ranges->length(); k++) {
          CharacterRange& range = ranges->at(k);
          if (range.from() > max_char) continue;
          int to = Min(max_char, static_cast<int>(range.to()));
          bm->SetInterval(offset, Interval(range.from(), to));
        }
      }
      offset++;
    }
  }
  if (offset >= bm->length()) {
    if (initial_offset == 0) set_bm_info(not_at_start, bm);
    return;
  }
  on_success()->FillInBMInfo(isolate, offset, budget - 1, bm,
                             true);  // Not at start after a text node.
  if (initial_offset == 0) set_bm_info(not_at_start, bm);
}


// -------------------------------------------------------------------
// Dispatch table construction


void DispatchTableConstructor::VisitEnd(EndNode* that) {
  AddRange(CharacterRange::Everything());
}


void DispatchTableConstructor::BuildTable(ChoiceNode* node) {
  node->set_being_calculated(true);
  ZoneList<GuardedAlternative>* alternatives = node->alternatives();
  for (int i = 0; i < alternatives->length(); i++) {
    set_choice_index(i);
    alternatives->at(i).node()->Accept(this);
  }
  node->set_being_calculated(false);
}


class AddDispatchRange {
 public:
  explicit AddDispatchRange(DispatchTableConstructor* constructor)
    : constructor_(constructor) { }
  void Call(uc32 from, DispatchTable::Entry entry);
 private:
  DispatchTableConstructor* constructor_;
};


void AddDispatchRange::Call(uc32 from, DispatchTable::Entry entry) {
  constructor_->AddRange(CharacterRange::Range(from, entry.to()));
}


void DispatchTableConstructor::VisitChoice(ChoiceNode* node) {
  if (node->being_calculated())
    return;
  DispatchTable* table = node->GetTable(ignore_case_);
  AddDispatchRange adder(this);
  table->ForEach(&adder);
}


void DispatchTableConstructor::VisitBackReference(BackReferenceNode* that) {
  // TODO(160): Find the node that we refer back to and propagate its start
  // set back to here.  For now we just accept anything.
  AddRange(CharacterRange::Everything());
}


void DispatchTableConstructor::VisitAssertion(AssertionNode* that) {
  RegExpNode* target = that->on_success();
  target->Accept(this);
}


static int CompareRangeByFrom(const CharacterRange* a,
                              const CharacterRange* b) {
  return Compare<uc16>(a->from(), b->from());
}


void DispatchTableConstructor::AddInverse(ZoneList<CharacterRange>* ranges) {
  ranges->Sort(CompareRangeByFrom);
  uc16 last = 0;
  for (int i = 0; i < ranges->length(); i++) {
    CharacterRange range = ranges->at(i);
    if (last < range.from())
      AddRange(CharacterRange::Range(last, range.from() - 1));
    if (range.to() >= last) {
      if (range.to() == String::kMaxCodePoint) {
        return;
      } else {
        last = range.to() + 1;
      }
    }
  }
  AddRange(CharacterRange::Range(last, String::kMaxCodePoint));
}


void DispatchTableConstructor::VisitText(TextNode* that) {
  TextElement elm = that->elements()->at(0);
  switch (elm.text_type()) {
    case TextElement::ATOM: {
      uc16 c = elm.atom()->data()[0];
      AddRange(CharacterRange::Range(c, c));
      break;
    }
    case TextElement::CHAR_CLASS: {
      RegExpCharacterClass* tree = elm.char_class();
      ZoneList<CharacterRange>* ranges = tree->ranges(that->zone());
      if (tree->is_negated()) {
        AddInverse(ranges);
      } else {
        for (int i = 0; i < ranges->length(); i++)
          AddRange(ranges->at(i));
      }
      break;
    }
    default: {
      UNIMPLEMENTED();
    }
  }
}


void DispatchTableConstructor::VisitAction(ActionNode* that) {
  RegExpNode* target = that->on_success();
  target->Accept(this);
}

RegExpNode* OptionallyStepBackToLeadSurrogate(RegExpCompiler* compiler,
                                              RegExpNode* on_success,
                                              JSRegExp::Flags flags) {
  // If the regexp matching starts within a surrogate pair, step back
  // to the lead surrogate and start matching from there.
  DCHECK(!compiler->read_backward());
  Zone* zone = compiler->zone();
  ZoneList<CharacterRange>* lead_surrogates = CharacterRange::List(
      zone, CharacterRange::Range(kLeadSurrogateStart, kLeadSurrogateEnd));
  ZoneList<CharacterRange>* trail_surrogates = CharacterRange::List(
      zone, CharacterRange::Range(kTrailSurrogateStart, kTrailSurrogateEnd));

  ChoiceNode* optional_step_back = new (zone) ChoiceNode(2, zone);

  int stack_register = compiler->UnicodeLookaroundStackRegister();
  int position_register = compiler->UnicodeLookaroundPositionRegister();
  RegExpNode* step_back = TextNode::CreateForCharacterRanges(
      zone, lead_surrogates, true, on_success, flags);
  RegExpLookaround::Builder builder(true, step_back, stack_register,
                                    position_register);
  RegExpNode* match_trail = TextNode::CreateForCharacterRanges(
      zone, trail_surrogates, false, builder.on_match_success(), flags);

  optional_step_back->AddAlternative(
      GuardedAlternative(builder.ForMatch(match_trail)));
  optional_step_back->AddAlternative(GuardedAlternative(on_success));

  return optional_step_back;
}


RegExpEngine::CompilationResult RegExpEngine::Compile(
    Isolate* isolate, Zone* zone, RegExpCompileData* data,
    JSRegExp::Flags flags, Handle<String> pattern,
    Handle<String> sample_subject, bool is_one_byte) {
  if ((data->capture_count + 1) * 2 - 1 > RegExpMacroAssembler::kMaxRegister) {
    return IrregexpRegExpTooBig(isolate);
  }
  bool is_sticky = IsSticky(flags);
  bool is_global = IsGlobal(flags);
  bool is_unicode = IsUnicode(flags);
  RegExpCompiler compiler(isolate, zone, data->capture_count, is_one_byte);

  if (compiler.optimize()) compiler.set_optimize(!TooMuchRegExpCode(pattern));

  // Sample some characters from the middle of the string.
  static const int kSampleSize = 128;

  sample_subject = String::Flatten(sample_subject);
  int chars_sampled = 0;
  int half_way = (sample_subject->length() - kSampleSize) / 2;
  for (int i = Max(0, half_way);
       i < sample_subject->length() && chars_sampled < kSampleSize;
       i++, chars_sampled++) {
    compiler.frequency_collator()->CountCharacter(sample_subject->Get(i));
  }

  // Wrap the body of the regexp in capture #0.
  RegExpNode* captured_body = RegExpCapture::ToNode(data->tree,
                                                    0,
                                                    &compiler,
                                                    compiler.accept());
  RegExpNode* node = captured_body;
  bool is_end_anchored = data->tree->IsAnchoredAtEnd();
  bool is_start_anchored = data->tree->IsAnchoredAtStart();
  int max_length = data->tree->max_match();
  if (!is_start_anchored && !is_sticky) {
    // Add a .*? at the beginning, outside the body capture, unless
    // this expression is anchored at the beginning or sticky.
    JSRegExp::Flags default_flags = JSRegExp::Flags();
    RegExpNode* loop_node = RegExpQuantifier::ToNode(
        0, RegExpTree::kInfinity, false,
        new (zone) RegExpCharacterClass('*', default_flags), &compiler,
        captured_body, data->contains_anchor);

    if (data->contains_anchor) {
      // Unroll loop once, to take care of the case that might start
      // at the start of input.
      ChoiceNode* first_step_node = new(zone) ChoiceNode(2, zone);
      first_step_node->AddAlternative(GuardedAlternative(captured_body));
      first_step_node->AddAlternative(GuardedAlternative(new (zone) TextNode(
          new (zone) RegExpCharacterClass('*', default_flags), false,
          loop_node)));
      node = first_step_node;
    } else {
      node = loop_node;
    }
  }
  if (is_one_byte) {
    node = node->FilterOneByte(RegExpCompiler::kMaxRecursion);
    // Do it again to propagate the new nodes to places where they were not
    // put because they had not been calculated yet.
    if (node != nullptr) {
      node = node->FilterOneByte(RegExpCompiler::kMaxRecursion);
    }
  } else if (is_unicode && (is_global || is_sticky)) {
    node = OptionallyStepBackToLeadSurrogate(&compiler, node, flags);
  }

  if (node == nullptr) node = new (zone) EndNode(EndNode::BACKTRACK, zone);
  data->node = node;
  Analysis analysis(isolate, is_one_byte);
  analysis.EnsureAnalyzed(node);
  if (analysis.has_failed()) {
    const char* error_message = analysis.error_message();
    return CompilationResult(isolate, error_message);
  }

  // Create the correct assembler for the architecture.
#ifndef V8_INTERPRETED_REGEXP
  // Native regexp implementation.

  NativeRegExpMacroAssembler::Mode mode =
      is_one_byte ? NativeRegExpMacroAssembler::LATIN1
                  : NativeRegExpMacroAssembler::UC16;

#if V8_TARGET_ARCH_IA32
  RegExpMacroAssemblerIA32 macro_assembler(isolate, zone, mode,
                                           (data->capture_count + 1) * 2);
#elif V8_TARGET_ARCH_X64
  RegExpMacroAssemblerX64 macro_assembler(isolate, zone, mode,
                                          (data->capture_count + 1) * 2);
#elif V8_TARGET_ARCH_ARM
  RegExpMacroAssemblerARM macro_assembler(isolate, zone, mode,
                                          (data->capture_count + 1) * 2);
#elif V8_TARGET_ARCH_ARM64
  RegExpMacroAssemblerARM64 macro_assembler(isolate, zone, mode,
                                            (data->capture_count + 1) * 2);
#elif V8_TARGET_ARCH_S390
  RegExpMacroAssemblerS390 macro_assembler(isolate, zone, mode,
                                           (data->capture_count + 1) * 2);
#elif V8_TARGET_ARCH_PPC
  RegExpMacroAssemblerPPC macro_assembler(isolate, zone, mode,
                                          (data->capture_count + 1) * 2);
#elif V8_TARGET_ARCH_MIPS
  RegExpMacroAssemblerMIPS macro_assembler(isolate, zone, mode,
                                           (data->capture_count + 1) * 2);
#elif V8_TARGET_ARCH_MIPS64
  RegExpMacroAssemblerMIPS macro_assembler(isolate, zone, mode,
                                           (data->capture_count + 1) * 2);
#else
#error "Unsupported architecture"
#endif

#else  // V8_INTERPRETED_REGEXP
  // Interpreted regexp implementation.
  EmbeddedVector<byte, 1024> codes;
  RegExpMacroAssemblerIrregexp macro_assembler(isolate, codes, zone);
#endif  // V8_INTERPRETED_REGEXP

  macro_assembler.set_slow_safe(TooMuchRegExpCode(pattern));

  // Inserted here, instead of in Assembler, because it depends on information
  // in the AST that isn't replicated in the Node structure.
  static const int kMaxBacksearchLimit = 1024;
  if (is_end_anchored && !is_start_anchored && !is_sticky &&
      max_length < kMaxBacksearchLimit) {
    macro_assembler.SetCurrentPositionFromEnd(max_length);
  }

  if (is_global) {
    RegExpMacroAssembler::GlobalMode mode = RegExpMacroAssembler::GLOBAL;
    if (data->tree->min_match() > 0) {
      mode = RegExpMacroAssembler::GLOBAL_NO_ZERO_LENGTH_CHECK;
    } else if (is_unicode) {
      mode = RegExpMacroAssembler::GLOBAL_UNICODE;
    }
    macro_assembler.set_global_mode(mode);
  }

  return compiler.Assemble(&macro_assembler,
                           node,
                           data->capture_count,
                           pattern);
}


bool RegExpEngine::TooMuchRegExpCode(Handle<String> pattern) {
  Heap* heap = pattern->GetHeap();
  bool too_much = pattern->length() > RegExpImpl::kRegExpTooLargeToOptimize;
  if (heap->isolate()->total_regexp_code_generated() >
          RegExpImpl::kRegExpCompiledLimit &&
      heap->CommittedMemoryExecutable() >
          RegExpImpl::kRegExpExecutableMemoryLimit) {
    too_much = true;
  }
  return too_much;
}


Object* RegExpResultsCache::Lookup(Heap* heap, String* key_string,
                                   Object* key_pattern,
                                   FixedArray** last_match_cache,
                                   ResultsCacheType type) {
  FixedArray* cache;
  if (!key_string->IsInternalizedString()) return Smi::kZero;
  if (type == STRING_SPLIT_SUBSTRINGS) {
    DCHECK(key_pattern->IsString());
    if (!key_pattern->IsInternalizedString()) return Smi::kZero;
    cache = heap->string_split_cache();
  } else {
    DCHECK(type == REGEXP_MULTIPLE_INDICES);
    DCHECK(key_pattern->IsFixedArray());
    cache = heap->regexp_multiple_cache();
  }

  uint32_t hash = key_string->Hash();
  uint32_t index = ((hash & (kRegExpResultsCacheSize - 1)) &
                    ~(kArrayEntriesPerCacheEntry - 1));
  if (cache->get(index + kStringOffset) != key_string ||
      cache->get(index + kPatternOffset) != key_pattern) {
    index =
        ((index + kArrayEntriesPerCacheEntry) & (kRegExpResultsCacheSize - 1));
    if (cache->get(index + kStringOffset) != key_string ||
        cache->get(index + kPatternOffset) != key_pattern) {
      return Smi::kZero;
    }
  }

  *last_match_cache = FixedArray::cast(cache->get(index + kLastMatchOffset));
  return cache->get(index + kArrayOffset);
}


void RegExpResultsCache::Enter(Isolate* isolate, Handle<String> key_string,
                               Handle<Object> key_pattern,
                               Handle<FixedArray> value_array,
                               Handle<FixedArray> last_match_cache,
                               ResultsCacheType type) {
  Factory* factory = isolate->factory();
  Handle<FixedArray> cache;
  if (!key_string->IsInternalizedString()) return;
  if (type == STRING_SPLIT_SUBSTRINGS) {
    DCHECK(key_pattern->IsString());
    if (!key_pattern->IsInternalizedString()) return;
    cache = factory->string_split_cache();
  } else {
    DCHECK(type == REGEXP_MULTIPLE_INDICES);
    DCHECK(key_pattern->IsFixedArray());
    cache = factory->regexp_multiple_cache();
  }

  uint32_t hash = key_string->Hash();
  uint32_t index = ((hash & (kRegExpResultsCacheSize - 1)) &
                    ~(kArrayEntriesPerCacheEntry - 1));
  if (cache->get(index + kStringOffset) == Smi::kZero) {
    cache->set(index + kStringOffset, *key_string);
    cache->set(index + kPatternOffset, *key_pattern);
    cache->set(index + kArrayOffset, *value_array);
    cache->set(index + kLastMatchOffset, *last_match_cache);
  } else {
    uint32_t index2 =
        ((index + kArrayEntriesPerCacheEntry) & (kRegExpResultsCacheSize - 1));
    if (cache->get(index2 + kStringOffset) == Smi::kZero) {
      cache->set(index2 + kStringOffset, *key_string);
      cache->set(index2 + kPatternOffset, *key_pattern);
      cache->set(index2 + kArrayOffset, *value_array);
      cache->set(index2 + kLastMatchOffset, *last_match_cache);
    } else {
      cache->set(index2 + kStringOffset, Smi::kZero);
      cache->set(index2 + kPatternOffset, Smi::kZero);
      cache->set(index2 + kArrayOffset, Smi::kZero);
      cache->set(index2 + kLastMatchOffset, Smi::kZero);
      cache->set(index + kStringOffset, *key_string);
      cache->set(index + kPatternOffset, *key_pattern);
      cache->set(index + kArrayOffset, *value_array);
      cache->set(index + kLastMatchOffset, *last_match_cache);
    }
  }
  // If the array is a reasonably short list of substrings, convert it into a
  // list of internalized strings.
  if (type == STRING_SPLIT_SUBSTRINGS && value_array->length() < 100) {
    for (int i = 0; i < value_array->length(); i++) {
      Handle<String> str(String::cast(value_array->get(i)), isolate);
      Handle<String> internalized_str = factory->InternalizeString(str);
      value_array->set(i, *internalized_str);
    }
  }
  // Convert backing store to a copy-on-write array.
  value_array->set_map_no_write_barrier(isolate->heap()->fixed_cow_array_map());
}


void RegExpResultsCache::Clear(FixedArray* cache) {
  for (int i = 0; i < kRegExpResultsCacheSize; i++) {
    cache->set(i, Smi::kZero);
  }
}

}  // namespace internal
}  // namespace v8
