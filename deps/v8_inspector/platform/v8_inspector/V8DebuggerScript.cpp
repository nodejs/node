// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8DebuggerScript.h"

namespace blink {

V8DebuggerScript::V8DebuggerScript()
    : m_startLine(0)
    , m_startColumn(0)
    , m_endLine(0)
    , m_endColumn(0)
    , m_executionContextId(0)
    , m_isContentScript(false)
    , m_isInternalScript(false)
    , m_isLiveEdit(false)
{
}

String16 V8DebuggerScript::sourceURL() const
{
    return m_sourceURL.isEmpty() ? m_url : m_sourceURL;
}

V8DebuggerScript& V8DebuggerScript::setURL(const String16& url)
{
    m_url = url;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setSourceURL(const String16& sourceURL)
{
    m_sourceURL = sourceURL;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setSourceMappingURL(const String16& sourceMappingURL)
{
    m_sourceMappingURL = sourceMappingURL;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setSource(const String16& source)
{
    m_source = source;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setHash(const String16& hash)
{
    m_hash = hash;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setStartLine(int startLine)
{
    m_startLine = startLine;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setStartColumn(int startColumn)
{
    m_startColumn = startColumn;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setEndLine(int endLine)
{
    m_endLine = endLine;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setEndColumn(int endColumn)
{
    m_endColumn = endColumn;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setExecutionContextId(int executionContextId)
{
    m_executionContextId = executionContextId;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setIsContentScript(bool isContentScript)
{
    m_isContentScript = isContentScript;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setIsInternalScript(bool isInternalScript)
{
    m_isInternalScript = isInternalScript;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setIsLiveEdit(bool isLiveEdit)
{
    m_isLiveEdit = isLiveEdit;
    return *this;
}

} // namespace blink
