// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Array_h
#define Array_h

#include "platform/inspector_protocol/ErrorSupport.h"
#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/ValueConversions.h"
#include "platform/inspector_protocol/Values.h"

#include <vector>

namespace blink {
namespace protocol {

template<typename T>
class Array {
public:
    static std::unique_ptr<Array<T>> create()
    {
        return wrapUnique(new Array<T>());
    }

    static std::unique_ptr<Array<T>> parse(protocol::Value* value, ErrorSupport* errors)
    {
        protocol::ListValue* array = ListValue::cast(value);
        if (!array) {
            errors->addError("array expected");
            return nullptr;
        }
        std::unique_ptr<Array<T>> result(new Array<T>());
        errors->push();
        for (size_t i = 0; i < array->size(); ++i) {
            errors->setName(String16::fromInteger(i));
            std::unique_ptr<T> item = ValueConversions<T>::parse(array->at(i), errors);
            result->m_vector.push_back(std::move(item));
        }
        errors->pop();
        if (errors->hasErrors())
            return nullptr;
        return result;
    }

    void addItem(std::unique_ptr<T> value)
    {
        m_vector.push_back(std::move(value));
    }

    size_t length()
    {
        return m_vector.size();
    }

    T* get(size_t index)
    {
        return m_vector[index].get();
    }

    std::unique_ptr<protocol::ListValue> serialize()
    {
        std::unique_ptr<protocol::ListValue> result = ListValue::create();
        for (auto& item : m_vector)
            result->pushValue(ValueConversions<T>::serialize(item));
        return result;
    }

private:
    std::vector<std::unique_ptr<T>> m_vector;
};

template<typename T>
class ArrayBase {
public:
    static std::unique_ptr<Array<T>> create()
    {
        return wrapUnique(new Array<T>());
    }

    static std::unique_ptr<Array<T>> parse(protocol::Value* value, ErrorSupport* errors)
    {
        protocol::ListValue* array = ListValue::cast(value);
        if (!array) {
            errors->addError("array expected");
            return nullptr;
        }
        errors->push();
        std::unique_ptr<Array<T>> result(new Array<T>());
        for (size_t i = 0; i < array->size(); ++i) {
            errors->setName(String16::fromInteger(i));
            T item = ValueConversions<T>::parse(array->at(i), errors);
            result->m_vector.push_back(item);
        }
        errors->pop();
        if (errors->hasErrors())
            return nullptr;
        return result;
    }

    void addItem(const T& value)
    {
        m_vector.push_back(value);
    }

    size_t length()
    {
        return m_vector.size();
    }

    T get(size_t index)
    {
        return m_vector[index];
    }

    std::unique_ptr<protocol::ListValue> serialize()
    {
        std::unique_ptr<protocol::ListValue> result = ListValue::create();
        for (auto& item : m_vector)
            result->pushValue(ValueConversions<T>::serialize(item));
        return result;
    }

private:
    std::vector<T> m_vector;
};

template<> class Array<String> : public ArrayBase<String> {};
template<> class Array<String16> : public ArrayBase<String16> {};
template<> class Array<int> : public ArrayBase<int> {};
template<> class Array<double> : public ArrayBase<double> {};
template<> class Array<bool> : public ArrayBase<bool> {};

} // namespace platform
} // namespace blink

#endif // !defined(Array_h)
