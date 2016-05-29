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

#ifndef V8DebuggerScript_h
#define V8DebuggerScript_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/String16.h"
#include <v8.h>

namespace blink {

class V8DebuggerScript {
    PROTOCOL_DISALLOW_NEW();
public:
    V8DebuggerScript();

    String16 url() const { return m_url; }
    bool hasSourceURL() const { return !m_sourceURL.isEmpty(); }
    String16 sourceURL() const;
    String16 sourceMappingURL() const { return m_sourceMappingURL; }
    String16 source() const { return m_source; }
    String16 hash() const { return m_hash; }
    int startLine() const { return m_startLine; }
    int startColumn() const { return m_startColumn; }
    int endLine() const { return m_endLine; }
    int endColumn() const { return m_endColumn; }
    int executionContextId() const { return m_executionContextId; }
    bool isContentScript() const { return m_isContentScript; }
    bool isInternalScript() const { return m_isInternalScript; }
    bool isLiveEdit() const { return m_isLiveEdit; }

    V8DebuggerScript& setURL(const String16&);
    V8DebuggerScript& setSourceURL(const String16&);
    V8DebuggerScript& setSourceMappingURL(const String16&);
    V8DebuggerScript& setSource(const String16&);
    V8DebuggerScript& setHash(const String16&);
    V8DebuggerScript& setStartLine(int);
    V8DebuggerScript& setStartColumn(int);
    V8DebuggerScript& setEndLine(int);
    V8DebuggerScript& setEndColumn(int);
    V8DebuggerScript& setExecutionContextId(int);
    V8DebuggerScript& setIsContentScript(bool);
    V8DebuggerScript& setIsInternalScript(bool);
    V8DebuggerScript& setIsLiveEdit(bool);

private:
    String16 m_url;
    String16 m_sourceURL;
    String16 m_sourceMappingURL;
    String16 m_source;
    String16 m_hash;
    int m_startLine;
    int m_startColumn;
    int m_endLine;
    int m_endColumn;
    int m_executionContextId;
    bool m_isContentScript;
    bool m_isInternalScript;
    bool m_isLiveEdit;
};

struct V8DebuggerParsedScript {
    String16 scriptId;
    V8DebuggerScript script;
    bool success;
};

} // namespace blink


#endif // V8DebuggerScript_h
