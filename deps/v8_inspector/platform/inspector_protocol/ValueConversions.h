// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ValueConversions_h
#define ValueConversions_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/ErrorSupport.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"

namespace blink {
namespace protocol {

PLATFORM_EXPORT std::unique_ptr<protocol::Value> toValue(int value);

PLATFORM_EXPORT std::unique_ptr<protocol::Value> toValue(double value);

PLATFORM_EXPORT std::unique_ptr<protocol::Value> toValue(bool value);

PLATFORM_EXPORT std::unique_ptr<protocol::Value> toValue(const String16& param);

PLATFORM_EXPORT std::unique_ptr<protocol::Value> toValue(const String& param);

PLATFORM_EXPORT std::unique_ptr<protocol::Value> toValue(protocol::Value* param);

PLATFORM_EXPORT std::unique_ptr<protocol::Value> toValue(protocol::DictionaryValue* param);

PLATFORM_EXPORT std::unique_ptr<protocol::Value> toValue(protocol::ListValue* param);

template<typename T> std::unique_ptr<protocol::Value> toValue(T* param)
{
    return param->serialize();
}

template<typename T> std::unique_ptr<protocol::Value> toValue(const std::unique_ptr<T>& param)
{
    static_assert(sizeof(T) == 0, "use raw pointer version.");
    return nullptr;
}

template<typename T> std::unique_ptr<protocol::Value> toValue(std::unique_ptr<T> param)
{
    static_assert(sizeof(T) == 0, "use raw pointer version.");
    return nullptr;
}

template<typename T>
struct FromValue {
    static std::unique_ptr<T> parse(protocol::Value* value, ErrorSupport* errors)
    {
        return T::parse(value, errors);
    }
};

template<>
struct FromValue<bool> {
    static bool parse(protocol::Value* value, ErrorSupport* errors)
    {
        bool result = false;
        bool success = value ? value->asBoolean(&result) : false;
        if (!success)
            errors->addError("boolean value expected");
        return result;
    }
};

template<>
struct FromValue<int> {
    static int parse(protocol::Value* value, ErrorSupport* errors)
    {
        int result = 0;
        bool success = value ? value->asNumber(&result) : false;
        if (!success)
            errors->addError("integer value expected");
        return result;
    }
};

template<>
struct FromValue<double> {
    static double parse(protocol::Value* value, ErrorSupport* errors)
    {
        double result = 0;
        bool success = value ? value->asNumber(&result) : false;
        if (!success)
            errors->addError("double value expected");
        return result;
    }
};

template<>
struct FromValue<String> {
    static String parse(protocol::Value* value, ErrorSupport* errors)
    {
        String16 result;
        bool success = value ? value->asString(&result) : false;
        if (!success)
            errors->addError("string value expected");
        return result;
    }
};

template<>
struct FromValue<String16> {
    static String16 parse(protocol::Value* value, ErrorSupport* errors)
    {
        String16 result;
        bool success = value ? value->asString(&result) : false;
        if (!success)
            errors->addError("string value expected");
        return result;
    }
};

template<>
struct FromValue<Value> {
    static std::unique_ptr<Value> parse(protocol::Value* value, ErrorSupport* errors)
    {
        bool success = !!value;
        if (!success)
            errors->addError("value expected");
        return value->clone();
    }
};

template<>
struct FromValue<DictionaryValue> {
    static std::unique_ptr<DictionaryValue> parse(protocol::Value* value, ErrorSupport* errors)
    {
        bool success = value && value->type() == protocol::Value::TypeObject;
        if (!success)
            errors->addError("object expected");
        return DictionaryValue::cast(value->clone());
    }
};

template<>
struct FromValue<ListValue> {
    static std::unique_ptr<ListValue> parse(protocol::Value* value, ErrorSupport* errors)
    {
        bool success = value && value->type() == protocol::Value::TypeArray;
        if (!success)
            errors->addError("list expected");
        return ListValue::cast(value->clone());
    }
};

template<typename T> class Array;

template<typename T>
struct FromValue<protocol::Array<T>> {
    static std::unique_ptr<protocol::Array<T>> parse(protocol::Value* value, ErrorSupport* errors)
    {
        return protocol::Array<T>::parse(value, errors);
    }
};

} // namespace platform
} // namespace blink

#endif // !defined(ValueConversions_h)
