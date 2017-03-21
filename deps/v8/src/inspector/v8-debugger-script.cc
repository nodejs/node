// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger-script.h"

#include "src/inspector/inspected-context.h"
#include "src/inspector/string-util.h"

namespace v8_inspector {

namespace {

const char hexDigits[17] = "0123456789ABCDEF";

void appendUnsignedAsHex(uint64_t number, String16Builder* destination) {
  for (size_t i = 0; i < 8; ++i) {
    UChar c = hexDigits[number & 0xF];
    destination->append(c);
    number >>= 4;
  }
}

// Hash algorithm for substrings is described in "Über die Komplexität der
// Multiplikation in
// eingeschränkten Branchingprogrammmodellen" by Woelfe.
// http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
String16 calculateHash(const String16& str) {
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
  const uint32_t* data = nullptr;
  size_t sizeInBytes = sizeof(UChar) * str.length();
  data = reinterpret_cast<const uint32_t*>(str.characters16());
  for (size_t i = 0; i < sizeInBytes / 4; i += 4) {
    uint32_t v = data[i];
    uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
    hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
    zi[current] = (zi[current] * random[current]) % prime[current];
    current = current == hashesSize - 1 ? 0 : current + 1;
  }
  if (sizeInBytes % 4) {
    uint32_t v = 0;
    for (size_t i = sizeInBytes - sizeInBytes % 4; i < sizeInBytes; ++i) {
      v <<= 8;
      v |= reinterpret_cast<const uint8_t*>(data)[i];
    }
    uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
    hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
    zi[current] = (zi[current] * random[current]) % prime[current];
    current = current == hashesSize - 1 ? 0 : current + 1;
  }

  for (size_t i = 0; i < hashesSize; ++i)
    hashes[i] = (hashes[i] + zi[i] * (prime[i] - 1)) % prime[i];

  String16Builder hash;
  for (size_t i = 0; i < hashesSize; ++i) appendUnsignedAsHex(hashes[i], &hash);
  return hash.toString();
}

class ActualScript : public V8DebuggerScript {
  friend class V8DebuggerScript;

 public:
  ActualScript(v8::Isolate* isolate, v8::Local<v8::debug::Script> script,
               bool isLiveEdit)
      : V8DebuggerScript(isolate, String16::fromInteger(script->Id()),
                         GetNameOrSourceUrl(script)),
        m_isLiveEdit(isLiveEdit) {
    v8::Local<v8::String> tmp;
    if (script->SourceURL().ToLocal(&tmp)) m_sourceURL = toProtocolString(tmp);
    if (script->SourceMappingURL().ToLocal(&tmp))
      m_sourceMappingURL = toProtocolString(tmp);
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

    v8::Local<v8::Value> contextData;
    if (script->ContextData().ToLocal(&contextData) && contextData->IsInt32()) {
      m_executionContextId =
          static_cast<int>(contextData.As<v8::Int32>()->Value());
    }

    if (script->Source().ToLocal(&tmp)) {
      m_sourceObj.Reset(m_isolate, tmp);
      String16 source = toProtocolString(tmp);
      // V8 will not count last line if script source ends with \n.
      if (source.length() > 1 && source[source.length() - 1] == '\n') {
        m_endLine++;
        m_endColumn = 0;
      }
    }

    m_script.Reset(m_isolate, script);
  }

  bool isLiveEdit() const override { return m_isLiveEdit; }

  const String16& sourceMappingURL() const override {
    return m_sourceMappingURL;
  }

  String16 source(v8::Isolate* isolate) const override {
    if (!m_sourceObj.IsEmpty())
      return toProtocolString(m_sourceObj.Get(isolate));
    return V8DebuggerScript::source(isolate);
  }

  void setSourceMappingURL(const String16& sourceMappingURL) override {
    m_sourceMappingURL = sourceMappingURL;
  }

  void setSource(v8::Local<v8::String> source) override {
    m_source = String16();
    m_sourceObj.Reset(m_isolate, source);
    m_hash = String16();
  }

  bool getPossibleBreakpoints(
      const v8::debug::Location& start, const v8::debug::Location& end,
      std::vector<v8::debug::Location>* locations) override {
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::debug::Script> script = m_script.Get(m_isolate);
    return script->GetPossibleBreakpoints(start, end, locations);
  }

 private:
  String16 GetNameOrSourceUrl(v8::Local<v8::debug::Script> script) {
    v8::Local<v8::String> name;
    if (script->Name().ToLocal(&name) || script->SourceURL().ToLocal(&name))
      return toProtocolString(name);
    return String16();
  }

  String16 m_sourceMappingURL;
  v8::Global<v8::String> m_sourceObj;
  bool m_isLiveEdit = false;
  v8::Global<v8::debug::Script> m_script;
};

class WasmVirtualScript : public V8DebuggerScript {
  friend class V8DebuggerScript;

 public:
  WasmVirtualScript(v8::Isolate* isolate,
                    v8::Local<v8::debug::WasmScript> script, String16 id,
                    String16 url, String16 source)
      : V8DebuggerScript(isolate, std::move(id), std::move(url)),
        m_script(isolate, script) {
    int num_lines = 0;
    int last_newline = -1;
    size_t next_newline = source.find('\n', last_newline + 1);
    while (next_newline != String16::kNotFound) {
      last_newline = static_cast<int>(next_newline);
      next_newline = source.find('\n', last_newline + 1);
      ++num_lines;
    }
    m_endLine = num_lines;
    m_endColumn = static_cast<int>(source.length()) - last_newline - 1;
    m_source = std::move(source);
  }

  const String16& sourceMappingURL() const override { return emptyString(); }
  bool isLiveEdit() const override { return false; }
  void setSourceMappingURL(const String16&) override {}

  bool getPossibleBreakpoints(
      const v8::debug::Location& start, const v8::debug::Location& end,
      std::vector<v8::debug::Location>* locations) override {
    // TODO(clemensh): Returning false produces the protocol error "Internal
    // error". Implement and fix expected output of
    // wasm-get-breakable-locations.js.
    return false;
  }

 private:
  static const String16& emptyString() {
    static const String16 singleEmptyString;
    return singleEmptyString;
  }

  v8::Global<v8::debug::WasmScript> m_script;
};

}  // namespace

std::unique_ptr<V8DebuggerScript> V8DebuggerScript::Create(
    v8::Isolate* isolate, v8::Local<v8::debug::Script> scriptObj,
    bool isLiveEdit) {
  return std::unique_ptr<ActualScript>(
      new ActualScript(isolate, scriptObj, isLiveEdit));
}

std::unique_ptr<V8DebuggerScript> V8DebuggerScript::CreateWasm(
    v8::Isolate* isolate, v8::Local<v8::debug::WasmScript> underlyingScript,
    String16 id, String16 url, String16 source) {
  return std::unique_ptr<WasmVirtualScript>(
      new WasmVirtualScript(isolate, underlyingScript, std::move(id),
                            std::move(url), std::move(source)));
}

V8DebuggerScript::V8DebuggerScript(v8::Isolate* isolate, String16 id,
                                   String16 url)
    : m_id(std::move(id)), m_url(std::move(url)), m_isolate(isolate) {}

V8DebuggerScript::~V8DebuggerScript() {}

const String16& V8DebuggerScript::sourceURL() const {
  return m_sourceURL.isEmpty() ? m_url : m_sourceURL;
}

const String16& V8DebuggerScript::hash(v8::Isolate* isolate) const {
  if (m_hash.isEmpty()) m_hash = calculateHash(source(isolate));
  DCHECK(!m_hash.isEmpty());
  return m_hash;
}

void V8DebuggerScript::setSourceURL(const String16& sourceURL) {
  m_sourceURL = sourceURL;
}

}  // namespace v8_inspector
