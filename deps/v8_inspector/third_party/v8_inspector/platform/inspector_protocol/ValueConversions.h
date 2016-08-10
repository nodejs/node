// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ValueConversions_h
#define ValueConversions_h

#include "platform/inspector_protocol/ErrorSupport.h"
#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"

namespace blink {
namespace protocol {

template<typename T>
struct ValueConversions {
    static std::unique_ptr<T> parse(protocol::Value* value, ErrorSupport* errors)
    {
        return T::parse(value, errors);
    }

    static std::unique_ptr<protocol::Value> serialize(T* value)
    {
        return value->serialize();
    }

    static std::unique_ptr<protocol::Value> serialize(const std::unique_ptr<T>& value)
    {
        return value->serialize();
    }
};

template<>
struct ValueConversions<bool> {
    static bool parse(protocol::Value* value, ErrorSupport* errors)
    {
        bool result = false;
        bool success = value ? value->asBoolean(&result) : false;
        if (!success)
            errors->addError("boolean value expected");
        return result;
    }

    static std::unique_ptr<protocol::Value> serialize(bool value)
    {
        return FundamentalValue::create(value);
    }
};

template<>
struct ValueConversions<int> {
    static int parse(protocol::Value* value, ErrorSupport* errors)
    {
        int result = 0;
        bool success = value ? value->asInteger(&result) : false;
        if (!success)
            errors->addError("integer value expected");
        return result;
    }

    static std::unique_ptr<protocol::Value> serialize(int value)
    {
        return FundamentalValue::create(value);
    }
};

template<>
struct ValueConversions<double> {
    static double parse(protocol::Value* value, ErrorSupport* errors)
    {
        double result = 0;
        bool success = value ? value->asDouble(&result) : false;
        if (!success)
            errors->addError("double value expected");
        return result;
    }

    static std::unique_ptr<protocol::Value> serialize(double value)
    {
        return FundamentalValue::create(value);
    }
};

template<>
struct ValueConversions<String> {
    static String parse(protocol::Value* value, ErrorSupport* errors)
    {
        String16 result;
        bool success = value ? value->asString(&result) : false;
        if (!success)
            errors->addError("string value expected");
        return result;
    }

    static std::unique_ptr<protocol::Value> serialize(const String& value)
    {
        return StringValue::create(value);
    }
};

template<>
struct ValueConversions<String16> {
    static String16 parse(protocol::Value* value, ErrorSupport* errors)
    {
        String16 result;
        bool success = value ? value->asString(&result) : false;
        if (!success)
            errors->addError("string value expected");
        return result;
    }

    static std::unique_ptr<protocol::Value> serialize(const String16& value)
    {
        return StringValue::create(value);
    }
};

template<>
struct ValueConversions<Value> {
    static std::unique_ptr<Value> parse(protocol::Value* value, ErrorSupport* errors)
    {
        bool success = !!value;
        if (!success) {
            errors->addError("value expected");
            return nullptr;
        }
        return value->clone();
    }

    static std::unique_ptr<protocol::Value> serialize(Value* value)
    {
        return value->clone();
    }

    static std::unique_ptr<protocol::Value> serialize(const std::unique_ptr<Value>& value)
    {
        return value->clone();
    }
};

template<>
struct ValueConversions<DictionaryValue> {
    static std::unique_ptr<DictionaryValue> parse(protocol::Value* value, ErrorSupport* errors)
    {
        bool success = value && value->type() == protocol::Value::TypeObject;
        if (!success)
            errors->addError("object expected");
        return DictionaryValue::cast(value->clone());
    }

    static std::unique_ptr<protocol::Value> serialize(DictionaryValue* value)
    {
        return value->clone();
    }

    static std::unique_ptr<protocol::Value> serialize(const std::unique_ptr<DictionaryValue>& value)
    {
        return value->clone();
    }
};

template<>
struct ValueConversions<ListValue> {
    static std::unique_ptr<ListValue> parse(protocol::Value* value, ErrorSupport* errors)
    {
        bool success = value && value->type() == protocol::Value::TypeArray;
        if (!success)
            errors->addError("list expected");
        return ListValue::cast(value->clone());
    }

    static std::unique_ptr<protocol::Value> serialize(ListValue* value)
    {
        return value->clone();
    }

    static std::unique_ptr<protocol::Value> serialize(const std::unique_ptr<ListValue>& value)
    {
        return value->clone();
    }
};

} // namespace platform
} // namespace blink

#endif // !defined(ValueConversions_h)
