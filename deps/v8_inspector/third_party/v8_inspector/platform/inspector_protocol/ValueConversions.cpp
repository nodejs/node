// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/ValueConversions.h"

namespace blink {
namespace protocol {

std::unique_ptr<protocol::Value> toValue(int value)
{
    return FundamentalValue::create(value);
}

std::unique_ptr<protocol::Value> toValue(double value)
{
    return FundamentalValue::create(value);
}

std::unique_ptr<protocol::Value> toValue(bool value)
{
    return FundamentalValue::create(value);
}

std::unique_ptr<protocol::Value> toValue(const String16& param)
{
    return StringValue::create(param);
}

std::unique_ptr<protocol::Value> toValue(const String& param)
{
    return StringValue::create(param);
}

std::unique_ptr<protocol::Value> toValue(Value* param)
{
    return param->clone();
}

std::unique_ptr<protocol::Value> toValue(DictionaryValue* param)
{
    return param->clone();
}

std::unique_ptr<protocol::Value> toValue(ListValue* param)
{
    return param->clone();
}

} // namespace protocol
} // namespace blink
