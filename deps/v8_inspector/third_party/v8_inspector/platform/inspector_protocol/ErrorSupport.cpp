// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/ErrorSupport.h"

#include "platform/inspector_protocol/String16.h"

namespace blink {
namespace protocol {

ErrorSupport::ErrorSupport() : m_errorString(nullptr) { }
ErrorSupport::ErrorSupport(String16* errorString) : m_errorString(errorString) { }
ErrorSupport::~ErrorSupport()
{
    if (m_errorString && hasErrors()) {
        String16Builder builder;
        builder.append("Internal error(s): ");
        builder.append(errors());
        *m_errorString = builder.toString();
    }
}

void ErrorSupport::setName(const String16& name)
{
    DCHECK(m_path.size());
    m_path[m_path.size() - 1] = name;
}

void ErrorSupport::push()
{
    m_path.push_back(String16());
}

void ErrorSupport::pop()
{
    m_path.pop_back();
}

void ErrorSupport::addError(const String16& error)
{
    String16Builder builder;
    for (size_t i = 0; i < m_path.size(); ++i) {
        if (i)
            builder.append('.');
        builder.append(m_path[i]);
    }
    builder.append(": ");
    builder.append(error);
    m_errors.push_back(builder.toString());
}

bool ErrorSupport::hasErrors()
{
    return m_errors.size();
}

String16 ErrorSupport::errors()
{
    String16Builder builder;
    for (size_t i = 0; i < m_errors.size(); ++i) {
        if (i)
            builder.append("; ");
        builder.append(m_errors[i]);
    }
    return builder.toString();
}

} // namespace protocol
} // namespace blink
