// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Object_h
#define Object_h

#include "platform/inspector_protocol/ErrorSupport.h"
#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/Values.h"

namespace blink {
namespace protocol {

class PLATFORM_EXPORT Object {
public:
    static std::unique_ptr<Object> parse(protocol::Value*, ErrorSupport*);
    ~Object();

    std::unique_ptr<protocol::DictionaryValue> serialize() const;
    std::unique_ptr<Object> clone() const;
private:
    explicit Object(std::unique_ptr<protocol::DictionaryValue>);
    std::unique_ptr<protocol::DictionaryValue> m_object;
};

} // namespace platform
} // namespace blink

#endif // !defined(Object_h)
