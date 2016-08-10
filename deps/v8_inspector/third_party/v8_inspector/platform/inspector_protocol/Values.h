// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Values_h
#define Values_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"

#include <vector>

namespace blink {
namespace protocol {

class ListValue;
class DictionaryValue;
class Value;

class PLATFORM_EXPORT Value {
    PROTOCOL_DISALLOW_COPY(Value);
public:
    static const int maxDepth = 1000;

    virtual ~Value() { }

    static std::unique_ptr<Value> null()
    {
        return wrapUnique(new Value());
    }

    enum ValueType {
        TypeNull = 0,
        TypeBoolean,
        TypeInteger,
        TypeDouble,
        TypeString,
        TypeObject,
        TypeArray,
        TypeSerialized
    };

    ValueType type() const { return m_type; }

    bool isNull() const { return m_type == TypeNull; }

    virtual bool asBoolean(bool* output) const;
    virtual bool asDouble(double* output) const;
    virtual bool asInteger(int* output) const;
    virtual bool asString(String16* output) const;
    virtual bool asSerialized(String16* output) const;

    String16 toJSONString() const;
    virtual void writeJSON(String16Builder* output) const;
    virtual std::unique_ptr<Value> clone() const;

protected:
    Value() : m_type(TypeNull) { }
    explicit Value(ValueType type) : m_type(type) { }

private:
    friend class DictionaryValue;
    friend class ListValue;

    ValueType m_type;
};

class PLATFORM_EXPORT FundamentalValue : public Value {
public:
    static std::unique_ptr<FundamentalValue> create(bool value)
    {
        return wrapUnique(new FundamentalValue(value));
    }

    static std::unique_ptr<FundamentalValue> create(int value)
    {
        return wrapUnique(new FundamentalValue(value));
    }

    static std::unique_ptr<FundamentalValue> create(double value)
    {
        return wrapUnique(new FundamentalValue(value));
    }

    bool asBoolean(bool* output) const override;
    bool asDouble(double* output) const override;
    bool asInteger(int* output) const override;
    void writeJSON(String16Builder* output) const override;
    std::unique_ptr<Value> clone() const override;

private:
    explicit FundamentalValue(bool value) : Value(TypeBoolean), m_boolValue(value) { }
    explicit FundamentalValue(int value) : Value(TypeInteger), m_integerValue(value) { }
    explicit FundamentalValue(double value) : Value(TypeDouble), m_doubleValue(value) { }

    union {
        bool m_boolValue;
        double m_doubleValue;
        int m_integerValue;
    };
};

class PLATFORM_EXPORT StringValue : public Value {
public:
    static std::unique_ptr<StringValue> create(const String16& value)
    {
        return wrapUnique(new StringValue(value));
    }

    static std::unique_ptr<StringValue> create(const char* value)
    {
        return wrapUnique(new StringValue(value));
    }

    bool asString(String16* output) const override;
    void writeJSON(String16Builder* output) const override;
    std::unique_ptr<Value> clone() const override;

private:
    explicit StringValue(const String16& value) : Value(TypeString), m_stringValue(value) { }
    explicit StringValue(const char* value) : Value(TypeString), m_stringValue(value) { }

    String16 m_stringValue;
};

class PLATFORM_EXPORT SerializedValue : public Value {
public:
    static std::unique_ptr<SerializedValue> create(const String16& value)
    {
        return wrapUnique(new SerializedValue(value));
    }

    bool asSerialized(String16* output) const override;
    void writeJSON(String16Builder* output) const override;
    std::unique_ptr<Value> clone() const override;

private:
    explicit SerializedValue(const String16& value) : Value(TypeSerialized), m_serializedValue(value) { }
    explicit SerializedValue(const char* value) : Value(TypeSerialized), m_serializedValue(value) { }

    String16 m_serializedValue;
};

class PLATFORM_EXPORT DictionaryValue : public Value {
public:
    using Entry = std::pair<String16, Value*>;
    static std::unique_ptr<DictionaryValue> create()
    {
        return wrapUnique(new DictionaryValue());
    }

    static DictionaryValue* cast(Value* value)
    {
        if (!value || value->type() != TypeObject)
            return nullptr;
        return static_cast<DictionaryValue*>(value);
    }

    static std::unique_ptr<DictionaryValue> cast(std::unique_ptr<Value> value)
    {
        return wrapUnique(DictionaryValue::cast(value.release()));
    }

    void writeJSON(String16Builder* output) const override;
    std::unique_ptr<Value> clone() const override;

    size_t size() const { return m_data.size(); }

    void setBoolean(const String16& name, bool);
    void setInteger(const String16& name, int);
    void setDouble(const String16& name, double);
    void setString(const String16& name, const String16&);
    void setValue(const String16& name, std::unique_ptr<Value>);
    void setObject(const String16& name, std::unique_ptr<DictionaryValue>);
    void setArray(const String16& name, std::unique_ptr<ListValue>);

    bool getBoolean(const String16& name, bool* output) const;
    bool getInteger(const String16& name, int* output) const;
    bool getDouble(const String16& name, double* output) const;
    bool getString(const String16& name, String16* output) const;

    DictionaryValue* getObject(const String16& name) const;
    ListValue* getArray(const String16& name) const;
    Value* get(const String16& name) const;
    Entry at(size_t index) const;

    bool booleanProperty(const String16& name, bool defaultValue) const;
    int integerProperty(const String16& name, int defaultValue) const;
    double doubleProperty(const String16& name, double defaultValue) const;
    void remove(const String16& name);

    ~DictionaryValue() override;

private:
    DictionaryValue();
    template<typename T>
    void set(const String16& key, std::unique_ptr<T>& value)
    {
        DCHECK(value);
        bool isNew = m_data.find(key) == m_data.end();
        m_data[key] = std::move(value);
        if (isNew)
            m_order.push_back(key);
    }

    using Dictionary = protocol::HashMap<String16, std::unique_ptr<Value>>;
    Dictionary m_data;
    std::vector<String16> m_order;
};

class PLATFORM_EXPORT ListValue : public Value {
public:
    static std::unique_ptr<ListValue> create()
    {
        return wrapUnique(new ListValue());
    }

    static ListValue* cast(Value* value)
    {
        if (!value || value->type() != TypeArray)
            return nullptr;
        return static_cast<ListValue*>(value);
    }

    static std::unique_ptr<ListValue> cast(std::unique_ptr<Value> value)
    {
        return wrapUnique(ListValue::cast(value.release()));
    }

    ~ListValue() override;

    void writeJSON(String16Builder* output) const override;
    std::unique_ptr<Value> clone() const override;

    void pushValue(std::unique_ptr<Value>);

    Value* at(size_t index);
    size_t size() const { return m_data.size(); }

private:
    ListValue();
    std::vector<std::unique_ptr<Value>> m_data;
};

} // namespace protocol
} // namespace blink

#endif // Values_h
