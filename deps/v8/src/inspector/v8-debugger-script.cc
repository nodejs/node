// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger-script.h"

#include "src/common/v8memory.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger-agent-impl.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/wasm-translation.h"

namespace v8_inspector {

namespace {

const char kGlobalDebuggerScriptHandleLabel[] = "DevTools debugger";

// Hash algorithm for substrings is described in "Über die Komplexität der
// Multiplikation in
// eingeschränkten Branchingprogrammmodellen" by Woelfe.
// http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
String16 calculateHash(v8::Isolate* isolate, v8::Local<v8::String> source) {
  static uint64_t prime[] = {0x3FB75161, 0xAB1F4E4F, 0x82675BC5, 0xCD924D35,
                             0x81ABE279};
  static uint64_t random[] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476,
                              0xC3D2E1F0};
  static uint32_t randomOdd[] = {0xB4663807, 0xCC322BF5, 0xD4F91BBD, 0xA7BEA11D,
                                 0x8F462907};

  uint64_t hashes[] = {0, 0, 0, 0, 0};
  uint64_t zi[] = {1, 1, 1, 1, 1};

  const size_t hashesSize = arraysize(hashes);

  size_t current = 0;

  std::unique_ptr<UChar[]> buffer(new UChar[source->Length()]);
  int written = source->Write(
      isolate, reinterpret_cast<uint16_t*>(buffer.get()), 0, source->Length());

  const uint32_t* data = nullptr;
  size_t sizeInBytes = sizeof(UChar) * written;
  data = reinterpret_cast<const uint32_t*>(buffer.get());
  for (size_t i = 0; i < sizeInBytes / 4; ++i) {
    uint32_t d = v8::internal::ReadUnalignedUInt32(
        reinterpret_cast<v8::internal::Address>(data + i));
#if V8_TARGET_LITTLE_ENDIAN
    uint32_t v = d;
#else
    uint32_t v = (d << 16) | (d >> 16);
#endif
    uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
    hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
    zi[current] = (zi[current] * random[current]) % prime[current];
    current = current == hashesSize - 1 ? 0 : current + 1;
  }
  if (sizeInBytes % 4) {
    uint32_t v = 0;
    const uint8_t* data_8b = reinterpret_cast<const uint8_t*>(data);
    for (size_t i = sizeInBytes - sizeInBytes % 4; i < sizeInBytes; ++i) {
      v <<= 8;
#if V8_TARGET_LITTLE_ENDIAN
      v |= data_8b[i];
#else
      if (i % 2) {
        v |= data_8b[i - 1];
      } else {
        v |= data_8b[i + 1];
      }
#endif
    }
    uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
    hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
    zi[current] = (zi[current] * random[current]) % prime[current];
    current = current == hashesSize - 1 ? 0 : current + 1;
  }

  for (size_t i = 0; i < hashesSize; ++i)
    hashes[i] = (hashes[i] + zi[i] * (prime[i] - 1)) % prime[i];

  String16Builder hash;
  for (size_t i = 0; i < hashesSize; ++i)
    hash.appendUnsignedAsHex(static_cast<uint32_t>(hashes[i]));
  return hash.toString();
}

void TranslateProtocolLocationToV8Location(WasmTranslation* wasmTranslation,
                                           v8::debug::Location* loc,
                                           const String16& scriptId,
                                           const String16& expectedV8ScriptId) {
  if (loc->IsEmpty()) return;
  int lineNumber = loc->GetLineNumber();
  int columnNumber = loc->GetColumnNumber();
  String16 translatedScriptId = scriptId;
  wasmTranslation->TranslateProtocolLocationToWasmScriptLocation(
      &translatedScriptId, &lineNumber, &columnNumber);
  DCHECK_EQ(expectedV8ScriptId.utf8(), translatedScriptId.utf8());
  *loc = v8::debug::Location(lineNumber, columnNumber);
}

void TranslateV8LocationToProtocolLocation(
    WasmTranslation* wasmTranslation, v8::debug::Location* loc,
    const String16& scriptId, const String16& expectedProtocolScriptId) {
  int lineNumber = loc->GetLineNumber();
  int columnNumber = loc->GetColumnNumber();
  String16 translatedScriptId = scriptId;
  wasmTranslation->TranslateWasmScriptLocationToProtocolLocation(
      &translatedScriptId, &lineNumber, &columnNumber);
  DCHECK_EQ(expectedProtocolScriptId.utf8(), translatedScriptId.utf8());
  *loc = v8::debug::Location(lineNumber, columnNumber);
}

class ActualScript : public V8DebuggerScript {
  friend class V8DebuggerScript;

 public:
  ActualScript(v8::Isolate* isolate, v8::Local<v8::debug::Script> script,
               bool isLiveEdit, V8DebuggerAgentImpl* agent,
               V8InspectorClient* client)
      : V8DebuggerScript(isolate, String16::fromInteger(script->Id()),
                         GetScriptURL(isolate, script, client)),
        m_agent(agent),
        m_isLiveEdit(isLiveEdit) {
    Initialize(script);
  }

  bool isLiveEdit() const override { return m_isLiveEdit; }
  bool isModule() const override { return m_isModule; }

  String16 source(size_t pos, size_t len) const override {
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::String> v8Source;
    if (!script()->Source().ToLocal(&v8Source)) return String16();
    if (pos >= static_cast<size_t>(v8Source->Length())) return String16();
    size_t substringLength =
        std::min(len, static_cast<size_t>(v8Source->Length()) - pos);
    std::unique_ptr<UChar[]> buffer(new UChar[substringLength]);
    v8Source->Write(m_isolate, reinterpret_cast<uint16_t*>(buffer.get()),
                    static_cast<int>(pos), static_cast<int>(substringLength));
    return String16(buffer.get(), substringLength);
  }
  int startLine() const override { return m_startLine; }
  int startColumn() const override { return m_startColumn; }
  int endLine() const override { return m_endLine; }
  int endColumn() const override { return m_endColumn; }
  bool isSourceLoadedLazily() const override { return false; }
  int length() const override {
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::String> v8Source;
    return script()->Source().ToLocal(&v8Source) ? v8Source->Length() : 0;
  }

  const String16& sourceMappingURL() const override {
    return m_sourceMappingURL;
  }

  void setSourceMappingURL(const String16& sourceMappingURL) override {
    m_sourceMappingURL = sourceMappingURL;
  }

  void setSource(const String16& newSource, bool preview,
                 v8::debug::LiveEditResult* result) override {
    v8::EscapableHandleScope scope(m_isolate);
    v8::Local<v8::String> v8Source = toV8String(m_isolate, newSource);
    if (!m_script.Get(m_isolate)->SetScriptSource(v8Source, preview, result)) {
      result->message = scope.Escape(result->message);
      return;
    }
    if (preview) return;
    m_hash = String16();
    Initialize(scope.Escape(result->script));
  }

  bool getPossibleBreakpoints(
      const v8::debug::Location& start, const v8::debug::Location& end,
      bool restrictToFunction,
      std::vector<v8::debug::BreakLocation>* locations) override {
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::debug::Script> script = m_script.Get(m_isolate);
    std::vector<v8::debug::BreakLocation> allLocations;
    if (!script->GetPossibleBreakpoints(start, end, restrictToFunction,
                                        &allLocations)) {
      return false;
    }
    if (!allLocations.size()) return true;
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
        DCHECK(
            allLocations[i].GetLineNumber() > current.GetLineNumber() ||
            (allLocations[i].GetColumnNumber() >= current.GetColumnNumber() &&
             allLocations[i].GetLineNumber() == current.GetLineNumber()));
        locations->push_back(current);
        current = allLocations[i];
      }
    }
    locations->push_back(current);
    return true;
  }

  void resetBlackboxedStateCache() override {
    v8::HandleScope scope(m_isolate);
    v8::debug::ResetBlackboxedStateCache(m_isolate, m_script.Get(m_isolate));
  }

  int offset(int lineNumber, int columnNumber) const override {
    v8::HandleScope scope(m_isolate);
    return m_script.Get(m_isolate)->GetSourceOffset(
        v8::debug::Location(lineNumber, columnNumber));
  }

  v8::debug::Location location(int offset) const override {
    v8::HandleScope scope(m_isolate);
    return m_script.Get(m_isolate)->GetSourceLocation(offset);
  }

  bool setBreakpoint(const String16& condition, v8::debug::Location* location,
                     int* id) const override {
    v8::HandleScope scope(m_isolate);
    return script()->SetBreakpoint(toV8String(m_isolate, condition), location,
                                   id);
  }

  bool setBreakpointOnRun(int* id) const override {
    v8::HandleScope scope(m_isolate);
    return script()->SetBreakpointOnScriptEntry(id);
  }

  const String16& hash() const override {
    if (!m_hash.isEmpty()) return m_hash;
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::String> v8Source;
    if (script()->Source().ToLocal(&v8Source)) {
      m_hash = calculateHash(m_isolate, v8Source);
    }
    DCHECK(!m_hash.isEmpty());
    return m_hash;
  }

 private:
  static String16 GetScriptURL(v8::Isolate* isolate,
                               v8::Local<v8::debug::Script> script,
                               V8InspectorClient* client) {
    v8::Local<v8::String> sourceURL;
    if (script->SourceURL().ToLocal(&sourceURL) && sourceURL->Length() > 0)
      return toProtocolString(isolate, sourceURL);
    v8::Local<v8::String> v8Name;
    if (script->Name().ToLocal(&v8Name) && v8Name->Length() > 0) {
      String16 name = toProtocolString(isolate, v8Name);
      std::unique_ptr<StringBuffer> url =
          client->resourceNameToUrl(toStringView(name));
      return url ? toString16(url->string()) : name;
    }
    return String16();
  }

  v8::Local<v8::debug::Script> script() const override {
    return m_script.Get(m_isolate);
  }

  void Initialize(v8::Local<v8::debug::Script> script) {
    v8::Local<v8::String> tmp;
    m_hasSourceURLComment =
        script->SourceURL().ToLocal(&tmp) && tmp->Length() > 0;
    if (script->SourceMappingURL().ToLocal(&tmp))
      m_sourceMappingURL = toProtocolString(m_isolate, tmp);
    m_startLine = script->LineOffset();
    m_startColumn = script->ColumnOffset();
    std::vector<int> lineEnds = script->LineEnds();
    CHECK(lineEnds.size());
    int source_length = lineEnds[lineEnds.size() - 1];
    if (lineEnds.size()) {
      m_endLine = static_cast<int>(lineEnds.size()) + m_startLine - 1;
      if (lineEnds.size() > 1) {
        m_endColumn = source_length - lineEnds[lineEnds.size() - 2] - 1;
      } else {
        m_endColumn = source_length + m_startColumn;
      }
    } else {
      m_endLine = m_startLine;
      m_endColumn = m_startColumn;
    }

    USE(script->ContextId().To(&m_executionContextId));

    m_isModule = script->IsModule();

    m_script.Reset(m_isolate, script);
    m_script.AnnotateStrongRetainer(kGlobalDebuggerScriptHandleLabel);
  }

  void MakeWeak() override {
    m_script.SetWeak(
        this,
        [](const v8::WeakCallbackInfo<ActualScript>& data) {
          data.GetParameter()->WeakCallback();
        },
        v8::WeakCallbackType::kFinalizer);
  }

  void WeakCallback() {
    m_script.ClearWeak();
    m_agent->ScriptCollected(this);
  }

  V8DebuggerAgentImpl* m_agent;
  String16 m_sourceMappingURL;
  bool m_isLiveEdit = false;
  bool m_isModule = false;
  mutable String16 m_hash;
  int m_startLine = 0;
  int m_startColumn = 0;
  int m_endLine = 0;
  int m_endColumn = 0;
  v8::Global<v8::debug::Script> m_script;
};

class WasmVirtualScript : public V8DebuggerScript {
  friend class V8DebuggerScript;

 public:
  WasmVirtualScript(v8::Isolate* isolate, WasmTranslation* wasmTranslation,
                    v8::Local<v8::debug::WasmScript> script, String16 id,
                    String16 url, int functionIndex)
      : V8DebuggerScript(isolate, std::move(id), std::move(url)),
        m_script(isolate, script),
        m_wasmTranslation(wasmTranslation),
        m_functionIndex(functionIndex) {
    m_script.AnnotateStrongRetainer(kGlobalDebuggerScriptHandleLabel);
    m_executionContextId = script->ContextId().ToChecked();
  }

  const String16& sourceMappingURL() const override { return emptyString(); }
  bool isLiveEdit() const override { return false; }
  bool isModule() const override { return false; }
  void setSourceMappingURL(const String16&) override {}
  void setSource(const String16&, bool, v8::debug::LiveEditResult*) override {
    UNREACHABLE();
  }
  bool isSourceLoadedLazily() const override { return true; }
  String16 source(size_t pos, size_t len) const override {
    return m_wasmTranslation->GetSource(m_id, m_functionIndex)
        .substring(pos, len);
  }
  int startLine() const override {
    return m_wasmTranslation->GetStartLine(m_id, m_functionIndex);
  }
  int startColumn() const override {
    return m_wasmTranslation->GetStartColumn(m_id, m_functionIndex);
  }
  int endLine() const override {
    return m_wasmTranslation->GetEndLine(m_id, m_functionIndex);
  }
  int endColumn() const override {
    return m_wasmTranslation->GetEndColumn(m_id, m_functionIndex);
  }
  int length() const override {
    return static_cast<int>(source(0, UINT_MAX).length());
  }

  bool getPossibleBreakpoints(
      const v8::debug::Location& start, const v8::debug::Location& end,
      bool restrictToFunction,
      std::vector<v8::debug::BreakLocation>* locations) override {
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::debug::Script> script = m_script.Get(m_isolate);
    String16 v8ScriptId = String16::fromInteger(script->Id());

    v8::debug::Location translatedStart = start;
    TranslateProtocolLocationToV8Location(m_wasmTranslation, &translatedStart,
                                          scriptId(), v8ScriptId);

    v8::debug::Location translatedEnd = end;
    if (translatedEnd.IsEmpty()) {
      // Stop before the start of the next function.
      translatedEnd =
          v8::debug::Location(translatedStart.GetLineNumber() + 1, 0);
    } else {
      TranslateProtocolLocationToV8Location(m_wasmTranslation, &translatedEnd,
                                            scriptId(), v8ScriptId);
    }

    bool success = script->GetPossibleBreakpoints(
        translatedStart, translatedEnd, restrictToFunction, locations);
    for (v8::debug::BreakLocation& loc : *locations) {
      TranslateV8LocationToProtocolLocation(m_wasmTranslation, &loc, v8ScriptId,
                                            scriptId());
    }
    return success;
  }

  void resetBlackboxedStateCache() override {}

  int offset(int lineNumber, int columnNumber) const override {
    return kNoOffset;
  }

  v8::debug::Location location(int offset) const override {
    return v8::debug::Location();
  }

  bool setBreakpoint(const String16& condition, v8::debug::Location* location,
                     int* id) const override {
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::debug::Script> script = m_script.Get(m_isolate);
    String16 v8ScriptId = String16::fromInteger(script->Id());

    TranslateProtocolLocationToV8Location(m_wasmTranslation, location,
                                          scriptId(), v8ScriptId);
    if (location->IsEmpty()) return false;
    if (!script->SetBreakpoint(toV8String(m_isolate, condition), location, id))
      return false;
    TranslateV8LocationToProtocolLocation(m_wasmTranslation, location,
                                          v8ScriptId, scriptId());
    return true;
  }

  bool setBreakpointOnRun(int*) const override { return false; }

  const String16& hash() const override {
    if (m_hash.isEmpty()) {
      m_hash = m_wasmTranslation->GetHash(m_id, m_functionIndex);
    }
    return m_hash;
  }

  void MakeWeak() override {}

 private:
  static const String16& emptyString() {
    // On the heap and leaked so that no destructor needs to run at exit time.
    static const String16* singleEmptyString = new String16;
    return *singleEmptyString;
  }

  v8::Local<v8::debug::Script> script() const override {
    return m_script.Get(m_isolate);
  }

  v8::Global<v8::debug::WasmScript> m_script;
  WasmTranslation* m_wasmTranslation;
  int m_functionIndex;
  mutable String16 m_hash;
};

}  // namespace

std::unique_ptr<V8DebuggerScript> V8DebuggerScript::Create(
    v8::Isolate* isolate, v8::Local<v8::debug::Script> scriptObj,
    bool isLiveEdit, V8DebuggerAgentImpl* agent, V8InspectorClient* client) {
  return v8::base::make_unique<ActualScript>(isolate, scriptObj, isLiveEdit,
                                             agent, client);
}

std::unique_ptr<V8DebuggerScript> V8DebuggerScript::CreateWasm(
    v8::Isolate* isolate, WasmTranslation* wasmTranslation,
    v8::Local<v8::debug::WasmScript> underlyingScript, String16 id,
    String16 url, int functionIndex) {
  return v8::base::make_unique<WasmVirtualScript>(
      isolate, wasmTranslation, underlyingScript, std::move(id), std::move(url),
      functionIndex);
}

V8DebuggerScript::V8DebuggerScript(v8::Isolate* isolate, String16 id,
                                   String16 url)
    : m_id(std::move(id)), m_url(std::move(url)), m_isolate(isolate) {}

V8DebuggerScript::~V8DebuggerScript() = default;

void V8DebuggerScript::setSourceURL(const String16& sourceURL) {
  if (sourceURL.length() > 0) {
    m_hasSourceURLComment = true;
    m_url = sourceURL;
  }
}

bool V8DebuggerScript::setBreakpoint(const String16& condition,
                                     v8::debug::Location* loc, int* id) const {
  v8::HandleScope scope(m_isolate);
  return script()->SetBreakpoint(toV8String(m_isolate, condition), loc, id);
}

}  // namespace v8_inspector
