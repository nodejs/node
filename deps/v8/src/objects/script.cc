// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/script.h"

#include "src/ast/ast.h"
#include "src/common/globals.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/tracing/traced-value.h"
#include "src/utils/hex-format.h"
#include "src/utils/sha-256.h"

namespace v8::internal {

template <typename IsolateT>
MaybeHandle<SharedFunctionInfo> Script::FindSharedFunctionInfo(
    DirectHandle<Script> script, IsolateT* isolate,
    FunctionLiteral* function_literal) {
  DCHECK(function_literal->shared_function_info().is_null());
  int function_literal_id = function_literal->function_literal_id();
  CHECK_GE(function_literal_id, 0);
  // If this check fails, the problem is most probably the function id
  // renumbering done by AstFunctionLiteralIdReindexer; in particular, that
  // AstTraversalVisitor doesn't recurse properly in the construct which
  // triggers the mismatch.
  CHECK_LT(function_literal_id, script->infos()->length());
  Tagged<MaybeObject> shared = script->infos()->get(function_literal_id);
  Tagged<HeapObject> heap_object;
  if (!shared.GetHeapObject(&heap_object) ||
      IsUndefined(heap_object, isolate)) {
    return MaybeHandle<SharedFunctionInfo>();
  }
  Handle<SharedFunctionInfo> result(Cast<SharedFunctionInfo>(heap_object),
                                    isolate);
  CHECK(Is<SharedFunctionInfo>(*result));
  CHECK_EQ(result->StartPosition(), function_literal->start_position());
  CHECK_EQ(result->EndPosition(), function_literal->end_position());
  CHECK_EQ(result->function_literal_id(kRelaxedLoad),
           function_literal->function_literal_id());
  function_literal->set_shared_function_info(result);
  return result;
}

template MaybeHandle<SharedFunctionInfo> Script::FindSharedFunctionInfo(
    DirectHandle<Script> script, Isolate* isolate,
    FunctionLiteral* function_literal);
template MaybeHandle<SharedFunctionInfo> Script::FindSharedFunctionInfo(
    DirectHandle<Script> script, LocalIsolate* isolate,
    FunctionLiteral* function_literal);

Script::Iterator::Iterator(Isolate* isolate)
    : iterator_(isolate->heap()->script_list()) {}

Tagged<Script> Script::Iterator::Next() {
  Tagged<Object> o = iterator_.Next();
  if (o != Tagged<Object>()) {
    return Cast<Script>(o);
  }
  return Script();
}

// static
int Script::GetEvalPosition(Isolate* isolate, DirectHandle<Script> script) {
  DCHECK(script->compilation_type() == Script::CompilationType::kEval);
  int position = script->eval_from_position();
  if (position < 0) {
    // Due to laziness, the position may not have been translated from code
    // offset yet, which would be encoded as negative integer. In that case,
    // translate and set the position.
    if (!script->has_eval_from_shared()) {
      position = 0;
    } else {
      DirectHandle<SharedFunctionInfo> shared(script->eval_from_shared(),
                                              isolate);
      SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, shared);
      position =
          shared->abstract_code(isolate)->SourcePosition(isolate, -position);
    }
    DCHECK_GE(position, 0);
    script->set_eval_from_position(position);
  }
  return position;
}

String::LineEndsVector Script::GetLineEnds(Isolate* isolate,
                                           DirectHandle<Script> script) {
  DCHECK(!script->has_line_ends());
  Tagged<Object> src_obj = script->source();
  if (IsString(src_obj)) {
    DirectHandle<String> src(Cast<String>(src_obj), isolate);
    return String::CalculateLineEndsVector(isolate, src, true);
  }

  return String::LineEndsVector();
}

template <typename IsolateT>
// static
void Script::InitLineEndsInternal(IsolateT* isolate,
                                  DirectHandle<Script> script) {
  DCHECK(!script->has_line_ends());
  DCHECK(script->CanHaveLineEnds());
  Tagged<Object> src_obj = script->source();
  if (!IsString(src_obj)) {
    DCHECK(IsUndefined(src_obj, isolate));
    script->set_line_ends(ReadOnlyRoots(isolate).empty_fixed_array());
  } else {
    DCHECK(IsString(src_obj));
    DirectHandle<String> src(Cast<String>(src_obj), isolate);
    DirectHandle<FixedArray> array =
        String::CalculateLineEnds(isolate, src, true);
    script->set_line_ends(*array);
  }
  DCHECK(IsFixedArray(script->line_ends()));
  DCHECK(script->has_line_ends());
}

void Script::SetSource(Isolate* isolate, DirectHandle<Script> script,
                       DirectHandle<String> source) {
  script->set_source(*source);
  if (isolate->NeedsSourcePositions()) {
    InitLineEnds(isolate, script);
  } else if (script->line_ends() ==
             ReadOnlyRoots(isolate).empty_fixed_array()) {
    DCHECK(script->has_line_ends());
    script->set_line_ends(Smi::zero());
    DCHECK(!script->has_line_ends());
  }
}

template V8_EXPORT_PRIVATE void Script::InitLineEndsInternal(
    Isolate* isolate, DirectHandle<Script> script);
template V8_EXPORT_PRIVATE void Script::InitLineEndsInternal(
    LocalIsolate* isolate, DirectHandle<Script> script);

bool Script::GetPositionInfo(DirectHandle<Script> script, int position,
                             PositionInfo* info, OffsetFlag offset_flag) {
#if V8_ENABLE_WEBASSEMBLY
  // For wasm, we do not create an artificial line_ends array, but do the
  // translation directly.
#ifdef DEBUG
  if (script->type() == Type::kWasm) {
    DCHECK(script->has_line_ends());
    DCHECK_EQ(Cast<FixedArray>(script->line_ends())->length(), 0);
  }
#endif  // DEBUG
#endif  // V8_ENABLE_WEBASSEMBLY
  InitLineEnds(Isolate::Current(), script);
  return script->GetPositionInfo(position, info, offset_flag);
}

bool Script::IsSubjectToDebugging() const {
  switch (type()) {
    case Type::kNormal:
#if V8_ENABLE_WEBASSEMBLY
    case Type::kWasm:
#endif  // V8_ENABLE_WEBASSEMBLY
      return true;
    case Type::kNative:
    case Type::kInspector:
    case Type::kExtension:
      return false;
  }
  UNREACHABLE();
}

bool Script::IsUserJavaScript() const {
  return type() == Script::Type::kNormal;
}

#if V8_ENABLE_WEBASSEMBLY
bool Script::ContainsAsmModule() {
  DisallowGarbageCollection no_gc;
  SharedFunctionInfo::ScriptIterator iter(Isolate::Current(), *this);
  for (Tagged<SharedFunctionInfo> sfi = iter.Next(); !sfi.is_null();
       sfi = iter.Next()) {
    if (sfi->HasAsmWasmData()) return true;
  }
  return false;
}
#endif  // V8_ENABLE_WEBASSEMBLY

void Script::TraceScriptRundown() {
  DisallowGarbageCollection no_gc;
  Isolate* isolate = Isolate::Current();
  Tagged<Object> context_value = isolate->native_context()->debug_context_id();
  int contextId = (IsSmi(context_value)) ? Smi::ToInt(context_value) : 0;
  auto value = v8::tracing::TracedValue::Create();
  value->SetInteger("scriptId", this->id());
  value->SetInteger("executionContextId", contextId);
  value->SetString("isolate", std::to_string(isolate->debug()->IsolateId()));
  value->SetBoolean("isModule", this->origin_options().IsModule());
  value->SetBoolean("hasSourceUrl", this->HasSourceURLComment());
  if (this->HasValidSource()) {
    if (this->HasSourceURLComment()) {
      value->SetString("sourceUrl",
                       Cast<String>(this->source_url())->ToCString().get());
    }
    if (this->HasSourceMappingURLComment()) {
      Tagged<String> sourceMapUrl = Cast<String>(this->source_mapping_url());
      // Source maps can be huge. Don't ever put a huge data url source map in
      // the trace. Also omit unusually large regular URLs.
      constexpr int kMaxSourceMapUrlLength = 2048;
      if (sourceMapUrl->length() > kMaxSourceMapUrlLength) {
        // Signal that there is a source map - clients may wish to request it by
        // other means (such as via CDP) or otherwise warn that a source map was
        // present but elided.
        value->SetBoolean("sourceMapUrlElided", true);
      } else {
        value->SetString("sourceMapUrl", sourceMapUrl->ToCString().get());
      }
    }
  }
  if (IsString(this->name())) {
    value->SetString("url", Cast<String>(this->name())->ToCString().get());
  }
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.v8-source-rundown"),
               "ScriptCatchup", "data", std::move(value));
}

void Script::TraceScriptRundownSources() {
  DisallowGarbageCollection no_gc;
  Isolate* isolate = Isolate::Current();
  if (!IsString(this->source())) return;
  Tagged<String> source = Cast<String>(this->source());
  auto script_id = this->id();
  int32_t source_length = source->length();

  const int32_t kSourceMaxLength = 25000000;  // 25mb
  const int32_t kSplitMaxLength = 1000000;    // 1mb
  if (source_length > kSourceMaxLength) {
    auto value = v8::tracing::TracedValue::Create();
    value->SetString("isolate", std::to_string(isolate->debug()->IsolateId()));
    value->SetInteger("scriptId", script_id);
    value->SetInteger("length", source_length);
    value->SetInteger("limit", kSourceMaxLength);
    TRACE_EVENT1(
        TRACE_DISABLED_BY_DEFAULT("devtools.v8-source-rundown-sources"),
        "TooLargeScriptCatchup", "data", std::move(value));
  } else if (source_length <= kSplitMaxLength) {
    auto value = v8::tracing::TracedValue::Create();
    value->SetString("isolate", std::to_string(isolate->debug()->IsolateId()));
    value->SetInteger("scriptId", script_id);
    value->SetInteger("length", source_length);
    value->SetString("sourceText", source->ToCString().get());
    TRACE_EVENT1(
        TRACE_DISABLED_BY_DEFAULT("devtools.v8-source-rundown-sources"),
        "ScriptCatchup", "data", std::move(value));
  } else {
    int32_t split_count = source_length / kSplitMaxLength + 1;
    std::unique_ptr<char[]> source_ptr = source->ToCString();
    for (int32_t i = 0; i < split_count; i++) {
      int32_t begin = i * kSplitMaxLength;
      int32_t end = std::min(begin + kSplitMaxLength, source_length);
      auto split_trace_value = v8::tracing::TracedValue::Create();
      split_trace_value->SetInteger("splitIndex", i);
      split_trace_value->SetInteger("splitCount", split_count);
      split_trace_value->SetString(
          "isolate", std::to_string(isolate->debug()->IsolateId()));
      split_trace_value->SetInteger("scriptId", script_id);
      split_trace_value->SetString(
          "sourceText", std::string(source_ptr.get() + begin, end - begin));
      TRACE_EVENT1(
          TRACE_DISABLED_BY_DEFAULT("devtools.v8-source-rundown-sources"),
          "LargeScriptCatchup", "data", std::move(split_trace_value));
    }
  }
}

void Script::AddPositionInfoOffset(PositionInfo* info,
                                   OffsetFlag offset_flag) const {
  // Add offsets if requested.
  if (offset_flag == OffsetFlag::kWithOffset) {
    if (info->line == 0) {
      info->column += column_offset();
    }
    info->line += line_offset();
  } else {
    DCHECK_EQ(offset_flag, OffsetFlag::kNoOffset);
  }
}

namespace {

template <typename Char>
bool GetPositionInfoSlowImpl(base::Vector<Char> source, int position,
                             Script::PositionInfo* info) {
  DCHECK(DisallowPositionInfoSlow::IsAllowed());
  if (position < 0) {
    position = 0;
  }
  int line = 0;
  const auto begin = std::cbegin(source);
  const auto end = std::cend(source);
  for (auto line_begin = begin; line_begin < end;) {
    const auto line_end = std::find(line_begin, end, '\n');
    if (position <= (line_end - begin)) {
      info->line = line;
      info->column = static_cast<int>((begin + position) - line_begin);
      info->line_start = static_cast<int>(line_begin - begin);
      info->line_end = static_cast<int>(line_end - begin);
      return true;
    }
    ++line;
    line_begin = line_end + 1;
  }
  return false;
}
bool GetPositionInfoSlow(const Tagged<Script> script, int position,
                         const DisallowGarbageCollection& no_gc,
                         Script::PositionInfo* info) {
  if (!IsString(script->source())) {
    return false;
  }
  auto source = Cast<String>(script->source());
  const auto flat = source->GetFlatContent(no_gc);
  return flat.IsOneByte()
             ? GetPositionInfoSlowImpl(flat.ToOneByteVector(), position, info)
             : GetPositionInfoSlowImpl(flat.ToUC16Vector(), position, info);
}

int GetLineEnd(const String::LineEndsVector& vector, int line) {
  return vector[line];
}

int GetLineEnd(const Tagged<FixedArray>& array, int line) {
  return Smi::ToInt(array->get(line));
}

int GetLength(const String::LineEndsVector& vector) {
  return static_cast<int>(vector.size());
}

int GetLength(const Tagged<FixedArray>& array) { return array->length(); }

template <typename LineEndsContainer>
bool GetLineEndsContainerPositionInfo(const LineEndsContainer& ends,
                                      int position, Script::PositionInfo* info,
                                      const DisallowGarbageCollection& no_gc) {
  const int ends_len = GetLength(ends);
  if (ends_len == 0) return false;

  // Return early on invalid positions. Negative positions behave as if 0 was
  // passed, and positions beyond the end of the script return as failure.
  if (position < 0) {
    position = 0;
  } else if (position > GetLineEnd(ends, ends_len - 1)) {
    return false;
  }

  // Determine line number by doing a binary search on the line ends array.
  if (GetLineEnd(ends, 0) >= position) {
    info->line = 0;
    info->line_start = 0;
    info->column = position;
  } else {
    int left = 0;
    int right = ends_len - 1;

    while (right > 0) {
      DCHECK_LE(left, right);
      const int mid = left + (right - left) / 2;
      if (position > GetLineEnd(ends, mid)) {
        left = mid + 1;
      } else if (position <= GetLineEnd(ends, mid - 1)) {
        right = mid - 1;
      } else {
        info->line = mid;
        break;
      }
    }
    DCHECK(GetLineEnd(ends, info->line) >= position &&
           GetLineEnd(ends, info->line - 1) < position);
    info->line_start = GetLineEnd(ends, info->line - 1) + 1;
    info->column = position - info->line_start;
  }

  return true;
}

}  // namespace

template <typename LineEndsContainer>
bool Script::GetPositionInfoInternal(
    const LineEndsContainer& ends, int position, Script::PositionInfo* info,
    const DisallowGarbageCollection& no_gc) const {
  if (!GetLineEndsContainerPositionInfo(ends, position, info, no_gc))
    return false;

  // Line end is position of the linebreak character.
  info->line_end = GetLineEnd(ends, info->line);
  if (info->line_end > 0) {
    DCHECK(IsString(source()));
    Tagged<String> src = Cast<String>(source());
    if (src->length() >= static_cast<uint32_t>(info->line_end) &&
        src->Get(info->line_end - 1) == '\r') {
      info->line_end--;
    }
  }

  return true;
}

template bool Script::GetPositionInfoInternal<String::LineEndsVector>(
    const String::LineEndsVector& ends, int position,
    Script::PositionInfo* info, const DisallowGarbageCollection& no_gc) const;
template bool Script::GetPositionInfoInternal<Tagged<FixedArray>>(
    const Tagged<FixedArray>& ends, int position, Script::PositionInfo* info,
    const DisallowGarbageCollection& no_gc) const;

bool Script::GetPositionInfo(int position, PositionInfo* info,
                             OffsetFlag offset_flag) const {
  DisallowGarbageCollection no_gc;

#if V8_ENABLE_WEBASSEMBLY
  // For wasm, we use the byte offset as the column.
  if (type() == Script::Type::kWasm) {
    DCHECK_LE(0, position);
    wasm::NativeModule* native_module = wasm_native_module();
    const wasm::WasmModule* module = native_module->module();
    if (module->functions.empty()) return false;
    info->line = 0;
    info->column = position;
    info->line_start = module->functions[0].code.offset();
    info->line_end = module->functions.back().code.end_offset();
    return true;
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  if (!has_line_ends()) {
    // Slow mode: we do not have line_ends. We have to iterate through source.
    if (!GetPositionInfoSlow(*this, position, no_gc, info)) {
      return false;
    }
  } else {
    DCHECK(has_line_ends());
    Tagged<FixedArray> ends = Cast<FixedArray>(line_ends());

    if (!GetPositionInfoInternal(ends, position, info, no_gc)) return false;
  }

  AddPositionInfoOffset(info, offset_flag);

  return true;
}

bool Script::GetPositionInfoWithLineEnds(
    int position, PositionInfo* info, const String::LineEndsVector& line_ends,
    OffsetFlag offset_flag) const {
  DisallowGarbageCollection no_gc;
  if (!GetPositionInfoInternal(line_ends, position, info, no_gc)) return false;

  AddPositionInfoOffset(info, offset_flag);

  return true;
}

bool Script::GetLineColumnWithLineEnds(
    int position, int& line, int& column,
    const String::LineEndsVector& line_ends) {
  DisallowGarbageCollection no_gc;
  PositionInfo info;
  if (!GetLineEndsContainerPositionInfo(line_ends, position, &info, no_gc)) {
    line = -1;
    column = -1;
    return false;
  }

  line = info.line;
  column = info.column;

  return true;
}

int Script::GetColumnNumber(DirectHandle<Script> script, int code_pos) {
  PositionInfo info;
  GetPositionInfo(script, code_pos, &info);
  return info.column;
}

int Script::GetColumnNumber(int code_pos) const {
  PositionInfo info;
  GetPositionInfo(code_pos, &info);
  return info.column;
}

int Script::GetLineNumber(DirectHandle<Script> script, int code_pos) {
  PositionInfo info;
  GetPositionInfo(script, code_pos, &info);
  return info.line;
}

int Script::GetLineNumber(int code_pos) const {
  PositionInfo info;
  GetPositionInfo(code_pos, &info);
  return info.line;
}

Tagged<Object> Script::GetNameOrSourceURL() {
  // Keep in sync with ScriptNameOrSourceURL in messages.js.
  if (!IsUndefined(source_url())) return source_url();
  return name();
}

// static
DirectHandle<String> Script::GetScriptHash(Isolate* isolate,
                                           DirectHandle<Script> script,
                                           bool forceForInspector) {
  if (script->origin_options().IsOpaque() && !forceForInspector) {
    return isolate->factory()->empty_string();
  }

  PtrComprCageBase cage_base(isolate);
  {
    Tagged<Object> maybe_source_hash = script->source_hash(cage_base);
    if (IsString(maybe_source_hash, cage_base)) {
      DirectHandle<String> precomputed(Cast<String>(maybe_source_hash),
                                       isolate);
      if (precomputed->length() > 0) {
        return precomputed;
      }
    }
  }

  DirectHandle<String> src_text;
  {
    Tagged<Object> maybe_script_source = script->source(cage_base);

    if (!IsString(maybe_script_source, cage_base)) {
      return isolate->factory()->empty_string();
    }
    src_text = direct_handle(Cast<String>(maybe_script_source), isolate);
  }

  char formatted_hash[kSizeOfFormattedSha256Digest];

  std::unique_ptr<char[]> string_val = src_text->ToCString();
  size_t len = strlen(string_val.get());
  uint8_t hash[kSizeOfSha256Digest];
  SHA256_hash(string_val.get(), len, hash);
  FormatBytesToHex(formatted_hash, kSizeOfFormattedSha256Digest, hash,
                   kSizeOfSha256Digest);
  formatted_hash[kSizeOfSha256Digest * 2] = '\0';

  DirectHandle<String> result =
      isolate->factory()->NewStringFromAsciiChecked(formatted_hash);
  script->set_source_hash(*result);
  return result;
}

}  // namespace v8::internal
