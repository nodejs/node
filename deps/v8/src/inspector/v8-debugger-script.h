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

#ifndef V8_INSPECTOR_V8DEBUGGERSCRIPT_H_
#define V8_INSPECTOR_V8DEBUGGERSCRIPT_H_

#include "src/base/macros.h"
#include "src/inspector/string-16.h"
#include "src/inspector/string-util.h"

#include "include/v8.h"
#include "src/debug/debug-interface.h"

namespace v8_inspector {

// Forward declaration.
class WasmTranslation;

class V8DebuggerScript {
 public:
  static std::unique_ptr<V8DebuggerScript> Create(
      v8::Isolate* isolate, v8::Local<v8::debug::Script> script,
      bool isLiveEdit);
  static std::unique_ptr<V8DebuggerScript> CreateWasm(
      v8::Isolate* isolate, WasmTranslation* wasmTranslation,
      v8::Local<v8::debug::WasmScript> underlyingScript, String16 id,
      String16 url, String16 source);

  virtual ~V8DebuggerScript();

  const String16& scriptId() const { return m_id; }
  const String16& url() const { return m_url; }
  bool hasSourceURL() const { return !m_sourceURL.isEmpty(); }
  const String16& sourceURL() const;
  virtual const String16& sourceMappingURL() const = 0;
  const String16& source() const { return m_source; }
  const String16& hash() const;
  int startLine() const { return m_startLine; }
  int startColumn() const { return m_startColumn; }
  int endLine() const { return m_endLine; }
  int endColumn() const { return m_endColumn; }
  int executionContextId() const { return m_executionContextId; }
  virtual bool isLiveEdit() const = 0;
  virtual bool isModule() const = 0;

  void setSourceURL(const String16&);
  virtual void setSourceMappingURL(const String16&) = 0;
  virtual void setSource(const String16& source, bool preview,
                         bool* stackChanged) = 0;

  virtual bool getPossibleBreakpoints(
      const v8::debug::Location& start, const v8::debug::Location& end,
      bool ignoreNestedFunctions,
      std::vector<v8::debug::BreakLocation>* locations) = 0;
  virtual void resetBlackboxedStateCache() = 0;

  static const int kNoOffset = -1;
  virtual int offset(int lineNumber, int columnNumber) const = 0;
  virtual v8::debug::Location location(int offset) const = 0;

  virtual bool setBreakpoint(const String16& condition,
                             v8::debug::Location* location, int* id) const = 0;

 protected:
  V8DebuggerScript(v8::Isolate*, String16 id, String16 url);

  virtual v8::Local<v8::debug::Script> script() const = 0;

  String16 m_id;
  String16 m_url;
  String16 m_sourceURL;
  String16 m_source;
  mutable String16 m_hash;
  int m_startLine = 0;
  int m_startColumn = 0;
  int m_endLine = 0;
  int m_endColumn = 0;
  int m_executionContextId = 0;

  v8::Isolate* m_isolate;

 private:
  DISALLOW_COPY_AND_ASSIGN(V8DebuggerScript);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8DEBUGGERSCRIPT_H_
