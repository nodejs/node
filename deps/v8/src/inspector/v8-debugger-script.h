/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8_INSPECTOR_V8_DEBUGGER_SCRIPT_H_
#define V8_INSPECTOR_V8_DEBUGGER_SCRIPT_H_

#include <memory>

#include "include/v8-local-handle.h"
#include "include/v8-maybe.h"
#include "src/base/macros.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/string-16.h"
#include "src/inspector/string-util.h"

namespace v8 {
class Isolate;
}

namespace v8_inspector {

class V8DebuggerAgentImpl;
class V8InspectorClient;

class V8DebuggerScript {
 public:
  enum class Language { JavaScript, WebAssembly };

  V8DebuggerScript(v8::Isolate* isolate, v8::Local<v8::debug::Script> script,
                   bool isLiveEdit, V8DebuggerAgentImpl* agent,
                   V8InspectorClient* client);
  ~V8DebuggerScript() = default;
  V8DebuggerScript(const V8DebuggerScript&) = delete;
  V8DebuggerScript& operator=(const V8DebuggerScript&) = delete;

  v8::Local<v8::debug::ScriptSource> scriptSource();
  const String16& scriptId() const { return m_id; }
  bool hasSourceURLComment() const { return m_hasSourceURLComment; }
  const String16& sourceURL() const { return m_url; }
  const String16& embedderName() const { return m_embedderName; }

  const String16& sourceMappingURL() const { return m_sourceMappingURL; }
  String16 source(size_t pos, size_t len = UINT_MAX) const;
  Language getLanguage() const { return m_language; }
  const String16& hash() const;
  String16 buildId() const;
  int startLine() const { return m_startLine; }
  int startColumn() const { return m_startColumn; }
  int endLine() const { return m_endLine; }
  int endColumn() const { return m_endColumn; }
  int codeOffset() const;
  int executionContextId() const { return m_executionContextId; }
  bool isLiveEdit() const { return m_isLiveEdit; }
  bool isModule() const { return m_isModule; }
  int length() const;

  void setSourceURL(const String16&);
  void setSourceMappingURL(const String16&);
  void setBuildId(const String16&);
  void setSource(const String16& source, bool preview,
                 bool allowTopFrameLiveEditing,
                 v8::debug::LiveEditResult* result);

  bool getPossibleBreakpoints(const v8::debug::Location& start,
                              const v8::debug::Location& end,
                              bool ignoreNestedFunctions,
                              std::vector<v8::debug::BreakLocation>* locations);
  void resetBlackboxedStateCache();

  v8::Maybe<int> offset(int lineNumber, int columnNumber) const;
  v8::debug::Location location(int offset) const;

  bool setBreakpoint(const String16& condition, v8::debug::Location* location,
                     int* id) const;
  void MakeWeak();
  void WeakCallback();
  bool setInstrumentationBreakpoint(int* id) const;

#if V8_ENABLE_WEBASSEMBLY
  v8::Maybe<v8::MemorySpan<const uint8_t>> wasmBytecode() const;
  std::vector<v8::debug::WasmScript::DebugSymbols> getDebugSymbols() const;
  void removeWasmBreakpoint(int id);
  void Disassemble(v8::debug::DisassemblyCollector* collector,
                   std::vector<int>* function_body_offsets) const;
#endif  // V8_ENABLE_WEBASSEMBLY

 private:
  v8::Local<v8::debug::Script> script() const;

  static String16 GetScriptURL(v8::Isolate* isolate,
                               v8::Local<v8::debug::Script> script,
                               V8InspectorClient* client);
  static String16 GetScriptName(v8::Isolate* isolate,
                                v8::Local<v8::debug::Script> script,
                                V8InspectorClient* client);
  void Initialize(v8::Local<v8::debug::Script> script);

  String16 m_id;
  String16 m_url;
  bool m_hasSourceURLComment = false;
  int m_executionContextId = 0;

  v8::Isolate* m_isolate;
  String16 m_embedderName;
  V8DebuggerAgentImpl* m_agent;
  String16 m_sourceMappingURL;
  mutable String16 m_buildId;
  Language m_language;
  bool m_isLiveEdit = false;
  bool m_isModule = false;
  mutable String16 m_hash;
  int m_startLine = 0;
  int m_startColumn = 0;
  int m_endLine = 0;
  int m_endColumn = 0;
  v8::Global<v8::debug::Script> m_script;
  v8::Global<v8::debug::ScriptSource> m_scriptSource;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_DEBUGGER_SCRIPT_H_
