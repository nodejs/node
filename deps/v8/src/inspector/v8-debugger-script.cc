// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger-script.h"

#include "src/inspector/protocol-platform.h"
#include "src/inspector/string-util.h"

namespace v8_inspector {

static const char hexDigits[17] = "0123456789ABCDEF";

static void appendUnsignedAsHex(uint64_t number, String16Builder* destination) {
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
static String16 calculateHash(const String16& str) {
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

V8DebuggerScript::V8DebuggerScript(v8::Isolate* isolate,
                                   v8::Local<v8::DebugInterface::Script> script,
                                   bool isLiveEdit) {
  m_isolate = script->GetIsolate();
  m_id = String16::fromInteger(script->Id());
  v8::Local<v8::String> tmp;
  if (script->Name().ToLocal(&tmp)) m_url = toProtocolString(tmp);
  if (script->SourceURL().ToLocal(&tmp)) {
    m_sourceURL = toProtocolString(tmp);
    if (m_url.isEmpty()) m_url = toProtocolString(tmp);
  }
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

  if (script->ContextData().ToLocal(&tmp)) {
    String16 contextData = toProtocolString(tmp);
    size_t firstComma = contextData.find(",", 0);
    size_t secondComma = firstComma != String16::kNotFound
                             ? contextData.find(",", firstComma + 1)
                             : String16::kNotFound;
    if (secondComma != String16::kNotFound) {
      String16 executionContextId =
          contextData.substring(firstComma + 1, secondComma - firstComma - 1);
      bool isOk = false;
      m_executionContextId = executionContextId.toInteger(&isOk);
      if (!isOk) m_executionContextId = 0;
      m_executionContextAuxData = contextData.substring(secondComma + 1);
    }
  }

  m_isLiveEdit = isLiveEdit;

  if (script->Source().ToLocal(&tmp)) {
    m_source.Reset(m_isolate, tmp);
    String16 source = toProtocolString(tmp);
    m_hash = calculateHash(source);
    // V8 will not count last line if script source ends with \n.
    if (source.length() > 1 && source[source.length() - 1] == '\n') {
      m_endLine++;
      m_endColumn = 0;
    }
  }

  m_script.Reset(m_isolate, script);
}

V8DebuggerScript::~V8DebuggerScript() {}

const String16& V8DebuggerScript::sourceURL() const {
  return m_sourceURL.isEmpty() ? m_url : m_sourceURL;
}

v8::Local<v8::String> V8DebuggerScript::source(v8::Isolate* isolate) const {
  return m_source.Get(isolate);
}

void V8DebuggerScript::setSourceURL(const String16& sourceURL) {
  m_sourceURL = sourceURL;
}

void V8DebuggerScript::setSourceMappingURL(const String16& sourceMappingURL) {
  m_sourceMappingURL = sourceMappingURL;
}

void V8DebuggerScript::setSource(v8::Local<v8::String> source) {
  m_source.Reset(m_isolate, source);
  m_hash = calculateHash(toProtocolString(source));
}

bool V8DebuggerScript::getPossibleBreakpoints(
    const v8::DebugInterface::Location& start,
    const v8::DebugInterface::Location& end,
    std::vector<v8::DebugInterface::Location>* locations) {
  v8::HandleScope scope(m_isolate);
  v8::Local<v8::DebugInterface::Script> script = m_script.Get(m_isolate);
  return script->GetPossibleBreakpoints(start, end, locations);
}

}  // namespace v8_inspector
