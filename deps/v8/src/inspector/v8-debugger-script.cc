// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger-script.h"

#include "src/base/memory.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Debugger.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger-agent-impl.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/utils/sha-256.h"

namespace v8_inspector {

namespace {

const char kGlobalDebuggerScriptHandleLabel[] = "DevTools debugger";

String16 calculateHash(v8::Isolate* isolate, v8::Local<v8::String> source) {
  uint32_t length = source->Length();
  std::unique_ptr<UChar[]> buffer(new UChar[length]);
  source->WriteV2(isolate, 0, length,
                  reinterpret_cast<uint16_t*>(buffer.get()));

  const uint8_t* data = nullptr;
  size_t sizeInBytes = sizeof(UChar) * length;
  data = reinterpret_cast<const uint8_t*>(buffer.get());

  uint8_t hash[kSizeOfSha256Digest];
  v8::internal::SHA256_hash(data, sizeInBytes, hash);

  String16Builder formatted_hash;
  for (size_t i = 0; i < kSizeOfSha256Digest; i++)
    formatted_hash.appendUnsignedAsHex(static_cast<uint8_t>(hash[i]));

  return formatted_hash.toString();
}

}  // namespace

V8DebuggerScript::V8DebuggerScript(v8::Isolate* isolate,
                                   v8::Local<v8::debug::Script> script,
                                   bool isLiveEdit, V8DebuggerAgentImpl* agent,
                                   V8InspectorClient* client)
    : m_id(String16::fromInteger(script->Id())),
      m_url(GetScriptURL(isolate, script, client)),
      m_isolate(isolate),
      m_embedderName(GetScriptName(isolate, script, client)),
      m_agent(agent),
      m_isLiveEdit(isLiveEdit) {
  Initialize(script);
}

String16 V8DebuggerScript::source(size_t pos, size_t len) const {
  v8::HandleScope scope(m_isolate);
  v8::Local<v8::String> v8Source;
  if (!m_scriptSource.Get(m_isolate)->JavaScriptCode().ToLocal(&v8Source)) {
    return String16();
  }
  if (pos >= static_cast<size_t>(v8Source->Length())) return String16();
  size_t substringLength =
      std::min(len, static_cast<size_t>(v8Source->Length()) - pos);
  std::unique_ptr<UChar[]> buffer(new UChar[substringLength]);
  v8Source->WriteV2(m_isolate, static_cast<uint32_t>(pos),
                    static_cast<uint32_t>(substringLength),
                    reinterpret_cast<uint16_t*>(buffer.get()));
  return String16(buffer.get(), substringLength);
}

#if V8_ENABLE_WEBASSEMBLY
v8::Maybe<v8::MemorySpan<const uint8_t>> V8DebuggerScript::wasmBytecode()
    const {
  v8::HandleScope scope(m_isolate);
  v8::MemorySpan<const uint8_t> bytecode;
  if (m_scriptSource.Get(m_isolate)->WasmBytecode().To(&bytecode)) {
    return v8::Just(bytecode);
  }
  return v8::Nothing<v8::MemorySpan<const uint8_t>>();
}

std::vector<v8::debug::WasmScript::DebugSymbols>
V8DebuggerScript::getDebugSymbols() const {
  auto script = this->script();
  if (!script->IsWasm())
    return std::vector<v8::debug::WasmScript::DebugSymbols>();
  return v8::debug::WasmScript::Cast(*script)->GetDebugSymbols();
}

void V8DebuggerScript::Disassemble(
    v8::debug::DisassemblyCollector* collector,
    std::vector<int>* function_body_offsets) const {
  v8::HandleScope scope(m_isolate);
  v8::Local<v8::debug::Script> script = this->script();
  DCHECK(script->IsWasm());
  v8::debug::WasmScript::Cast(*script)->Disassemble(collector,
                                                    function_body_offsets);
}
#endif  // V8_ENABLE_WEBASSEMBLY

int V8DebuggerScript::codeOffset() const {
#if V8_ENABLE_WEBASSEMBLY
    if (script()->IsWasm()) {
      return v8::debug::WasmScript::Cast(*script())->CodeOffset();
    }
#endif  // V8_ENABLE_WEBASSEMBLY
    return 0;
}
int V8DebuggerScript::length() const {
  return static_cast<int>(m_scriptSource.Get(m_isolate)->Length());
}

void V8DebuggerScript::setSourceMappingURL(const String16& sourceMappingURL) {
  m_sourceMappingURL = sourceMappingURL;
}

void V8DebuggerScript::setSource(const String16& newSource, bool preview,
                                 bool allowTopFrameLiveEditing,
                                 v8::debug::LiveEditResult* result) {
  v8::EscapableHandleScope scope(m_isolate);
  v8::Local<v8::String> v8Source = toV8String(m_isolate, newSource);
  if (!m_script.Get(m_isolate)->SetScriptSource(
          v8Source, preview, allowTopFrameLiveEditing, result)) {
    result->message = scope.Escape(result->message);
    return;
  }
  // NOP if preview or unchanged source (diffs.empty() in PatchScript)
  if (preview || result->script.IsEmpty()) return;

  m_hash = String16();
  Initialize(scope.Escape(result->script));
}

bool V8DebuggerScript::getPossibleBreakpoints(
    const v8::debug::Location& start, const v8::debug::Location& end,
    bool restrictToFunction, std::vector<v8::debug::BreakLocation>* locations) {
  v8::HandleScope scope(m_isolate);
  v8::Local<v8::debug::Script> script = m_script.Get(m_isolate);
  std::vector<v8::debug::BreakLocation> allLocations;
  if (!script->GetPossibleBreakpoints(start, end, restrictToFunction,
                                      &allLocations)) {
    return false;
  }
  if (allLocations.empty()) return true;
  v8::debug::BreakLocation current = allLocations[0];
  for (size_t i = 1; i < allLocations.size(); ++i) {
    if (allLocations[i].GetLineNumber() == current.GetLineNumber() &&
        allLocations[i].GetColumnNumber() == current.GetColumnNumber()) {
      if (allLocations[i].type() != v8::debug::kCommonBreakLocation) {
        DCHECK(allLocations[i].type() == v8::debug::kCallBreakLocation ||
               allLocations[i].type() == v8::debug::kReturnBreakLocation);
        // debugger can returns more then one break location at the same
        // source location, e.g. foo() - in this case there are two break
        // locations before foo: for statement and for function call, we can
        // merge them for inspector and report only one with call type.
        current = allLocations[i];
      }
    } else {
      // we assume that returned break locations are sorted.
      DCHECK(allLocations[i].GetLineNumber() > current.GetLineNumber() ||
             (allLocations[i].GetColumnNumber() >= current.GetColumnNumber() &&
              allLocations[i].GetLineNumber() == current.GetLineNumber()));
      locations->push_back(current);
      current = allLocations[i];
    }
  }
  locations->push_back(current);
  return true;
}

void V8DebuggerScript::resetBlackboxedStateCache() {
  v8::HandleScope scope(m_isolate);
  v8::debug::ResetBlackboxedStateCache(m_isolate, m_script.Get(m_isolate));
}

v8::Maybe<int> V8DebuggerScript::offset(int lineNumber,
                                        int columnNumber) const {
  v8::HandleScope scope(m_isolate);
  return m_script.Get(m_isolate)->GetSourceOffset(
      v8::debug::Location(lineNumber, columnNumber));
}

v8::debug::Location V8DebuggerScript::location(int offset) const {
  v8::HandleScope scope(m_isolate);
  return m_script.Get(m_isolate)->GetSourceLocation(offset);
}

bool V8DebuggerScript::setBreakpoint(const String16& condition,
                                     v8::debug::Location* location,
                                     int* id) const {
  v8::HandleScope scope(m_isolate);
  return script()->SetBreakpoint(toV8String(m_isolate, condition), location,
                                 id);
}

bool V8DebuggerScript::setInstrumentationBreakpoint(int* id) const {
  v8::HandleScope scope(m_isolate);
  return script()->SetInstrumentationBreakpoint(id);
}

const String16& V8DebuggerScript::hash() const {
  if (!m_hash.isEmpty()) return m_hash;
  v8::HandleScope scope(m_isolate);
  v8::Local<v8::String> v8Source;
  if (!m_scriptSource.Get(m_isolate)->JavaScriptCode().ToLocal(&v8Source)) {
    v8Source = v8::String::Empty(m_isolate);
  }
  m_hash = calculateHash(m_isolate, v8Source);
  DCHECK(!m_hash.isEmpty());
  return m_hash;
}

String16 V8DebuggerScript::buildId() const {
  if (!m_buildId.isEmpty()) return m_buildId;
  v8::Local<v8::debug::Script> script = this->script();
#if V8_ENABLE_WEBASSEMBLY
    if (m_language == Language::WebAssembly) {
      auto maybe_build_id =
          v8::debug::WasmScript::Cast(*script)->GetModuleBuildId();
      if (maybe_build_id.IsJust()) {
        v8::MemorySpan<const uint8_t> buildId = maybe_build_id.FromJust();
        String16Builder buildIdFormatter;
        for (size_t i = 0; i < buildId.size(); i++) {
          buildIdFormatter.appendUnsignedAsHex(
              static_cast<uint8_t>(buildId[i]));
        }
        m_buildId = buildIdFormatter.toString();
      }
    }
#endif  // V8_ENABLE_WEBASSEMBLY
    if (m_language == Language::JavaScript) {
      v8::Local<v8::String> debugId;
      if (script->DebugId().ToLocal(&debugId)) {
        m_buildId = toProtocolString(m_isolate, debugId);
      }
    }
    return m_buildId;
}

void V8DebuggerScript::setBuildId(const String16& buildId) {
  m_buildId = buildId;
}

// static
String16 V8DebuggerScript::GetScriptURL(v8::Isolate* isolate,
                                        v8::Local<v8::debug::Script> script,
                                        V8InspectorClient* client) {
  v8::Local<v8::String> sourceURL;
  if (script->SourceURL().ToLocal(&sourceURL) && sourceURL->Length() > 0)
    return toProtocolString(isolate, sourceURL);
  return GetScriptName(isolate, script, client);
}

// static
String16 V8DebuggerScript::GetScriptName(v8::Isolate* isolate,
                                         v8::Local<v8::debug::Script> script,
                                         V8InspectorClient* client) {
  v8::Local<v8::String> v8Name;
  if (script->Name().ToLocal(&v8Name) && v8Name->Length() > 0) {
    String16 name = toProtocolString(isolate, v8Name);
    std::unique_ptr<StringBuffer> url =
        client->resourceNameToUrl(toStringView(name));
    return url ? toString16(url->string()) : name;
  }
  return String16();
}

v8::Local<v8::debug::Script> V8DebuggerScript::script() const {
  return m_script.Get(m_isolate);
}

void V8DebuggerScript::Initialize(v8::Local<v8::debug::Script> script) {
  v8::Local<v8::String> tmp;
  m_hasSourceURLComment =
      script->SourceURL().ToLocal(&tmp) && tmp->Length() > 0;
  if (script->SourceMappingURL().ToLocal(&tmp))
    m_sourceMappingURL = toProtocolString(m_isolate, tmp);
  m_startLine = script->StartLine();
  m_startColumn = script->StartColumn();
  m_endLine = script->EndLine();
  m_endColumn = script->EndColumn();

  USE(script->ContextId().To(&m_executionContextId));
  m_language = V8DebuggerScript::Language::JavaScript;
#if V8_ENABLE_WEBASSEMBLY
    if (script->IsWasm()) {
      m_language = V8DebuggerScript::Language::WebAssembly;
    }
#endif  // V8_ENABLE_WEBASSEMBLY

    m_isModule = script->IsModule();

    bool hasHash = script->GetSha256Hash().ToLocal(&tmp) && tmp->Length() > 0;
    if (hasHash) {
      m_hash = toProtocolString(m_isolate, tmp);
    }

    m_script.Reset(m_isolate, script);
    m_script.AnnotateStrongRetainer(kGlobalDebuggerScriptHandleLabel);
    m_scriptSource.Reset(m_isolate, script->Source());
    m_scriptSource.AnnotateStrongRetainer(kGlobalDebuggerScriptHandleLabel);
}

void V8DebuggerScript::MakeWeak() {
  m_script.SetWeak(
      this,
      [](const v8::WeakCallbackInfo<V8DebuggerScript>& data) {
        data.GetParameter()->WeakCallback();
      },
      v8::WeakCallbackType::kParameter);
}

void V8DebuggerScript::WeakCallback() {
  m_script.Reset();
  m_agent->ScriptCollected(this);
}

void V8DebuggerScript::setSourceURL(const String16& sourceURL) {
  if (sourceURL.length() > 0) {
    m_hasSourceURLComment = true;
    m_url = sourceURL;
  }
}

#if V8_ENABLE_WEBASSEMBLY
void V8DebuggerScript::removeWasmBreakpoint(int id) {
  v8::HandleScope scope(m_isolate);
  script()->RemoveWasmBreakpoint(id);
}
#endif  // V8_ENABLE_WEBASSEMBLY

}  // namespace v8_inspector
