// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_Collections_h
#define node_inspector_protocol_Collections_h

#include "inspector/node_protocol/Forward.h"
#include <cstddef>

#if defined(__APPLE__) && !defined(_LIBCPP_VERSION)
#include <map>
#include <set>

namespace node {
namespace inspector {
namespace protocol {

template <class Key, class T> using HashMap = std::map<Key, T>;
template <class Key> using HashSet = std::set<Key>;

} // namespace node
} // namespace inspector
} // namespace protocol

#else
#include <unordered_map>
#include <unordered_set>

namespace node {
namespace inspector {
namespace protocol {

template <class Key, class T> using HashMap = std::unordered_map<Key, T>;
template <class Key> using HashSet = std::unordered_set<Key>;

} // namespace node
} // namespace inspector
} // namespace protocol

#endif // defined(__APPLE__) && !defined(_LIBCPP_VERSION)

#endif // !defined(node_inspector_protocol_Collections_h)


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_ErrorSupport_h
#define node_inspector_protocol_ErrorSupport_h

//#include "Forward.h"

namespace node {
namespace inspector {
namespace protocol {

class  ErrorSupport {
public:
    ErrorSupport();
    ~ErrorSupport();

    void push();
    void setName(const char*);
    void setName(const String&);
    void pop();
    void addError(const char*);
    void addError(const String&);
    bool hasErrors();
    String errors();

private:
    std::vector<String> m_path;
    std::vector<String> m_errors;
};

} // namespace node
} // namespace inspector
} // namespace protocol

#endif // !defined(node_inspector_protocol_ErrorSupport_h)


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_Values_h
#define node_inspector_protocol_Values_h

//#include "Allocator.h"
//#include "Collections.h"
//#include "Forward.h"

namespace node {
namespace inspector {
namespace protocol {

class ListValue;
class DictionaryValue;
class Value;

class  Value : public Serializable {
    PROTOCOL_DISALLOW_COPY(Value);
public:
    virtual ~Value() override { }

    static std::unique_ptr<Value> null()
    {
        return std::unique_ptr<Value>(new Value());
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
    virtual bool asString(String* output) const;
    virtual bool asSerialized(String* output) const;

    virtual void writeJSON(StringBuilder* output) const;
    virtual std::unique_ptr<Value> clone() const;
    String serialize() override;

protected:
    Value() : m_type(TypeNull) { }
    explicit Value(ValueType type) : m_type(type) { }

private:
    friend class DictionaryValue;
    friend class ListValue;

    ValueType m_type;
};

class  FundamentalValue : public Value {
public:
    static std::unique_ptr<FundamentalValue> create(bool value)
    {
        return std::unique_ptr<FundamentalValue>(new FundamentalValue(value));
    }

    static std::unique_ptr<FundamentalValue> create(int value)
    {
        return std::unique_ptr<FundamentalValue>(new FundamentalValue(value));
    }

    static std::unique_ptr<FundamentalValue> create(double value)
    {
        return std::unique_ptr<FundamentalValue>(new FundamentalValue(value));
    }

    bool asBoolean(bool* output) const override;
    bool asDouble(double* output) const override;
    bool asInteger(int* output) const override;
    void writeJSON(StringBuilder* output) const override;
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

class  StringValue : public Value {
public:
    static std::unique_ptr<StringValue> create(const String& value)
    {
        return std::unique_ptr<StringValue>(new StringValue(value));
    }

    static std::unique_ptr<StringValue> create(const char* value)
    {
        return std::unique_ptr<StringValue>(new StringValue(value));
    }

    bool asString(String* output) const override;
    void writeJSON(StringBuilder* output) const override;
    std::unique_ptr<Value> clone() const override;

private:
    explicit StringValue(const String& value) : Value(TypeString), m_stringValue(value) { }
    explicit StringValue(const char* value) : Value(TypeString), m_stringValue(value) { }

    String m_stringValue;
};

class  SerializedValue : public Value {
public:
    static std::unique_ptr<SerializedValue> create(const String& value)
    {
        return std::unique_ptr<SerializedValue>(new SerializedValue(value));
    }

    bool asSerialized(String* output) const override;
    void writeJSON(StringBuilder* output) const override;
    std::unique_ptr<Value> clone() const override;

private:
    explicit SerializedValue(const String& value) : Value(TypeSerialized), m_serializedValue(value) { }

    String m_serializedValue;
};

class  DictionaryValue : public Value {
public:
    using Entry = std::pair<String, Value*>;
    static std::unique_ptr<DictionaryValue> create()
    {
        return std::unique_ptr<DictionaryValue>(new DictionaryValue());
    }

    static DictionaryValue* cast(Value* value)
    {
        if (!value || value->type() != TypeObject)
            return nullptr;
        return static_cast<DictionaryValue*>(value);
    }

    static std::unique_ptr<DictionaryValue> cast(std::unique_ptr<Value> value)
    {
        return std::unique_ptr<DictionaryValue>(DictionaryValue::cast(value.release()));
    }

    void writeJSON(StringBuilder* output) const override;
    std::unique_ptr<Value> clone() const override;

    size_t size() const { return m_data.size(); }

    void setBoolean(const String& name, bool);
    void setInteger(const String& name, int);
    void setDouble(const String& name, double);
    void setString(const String& name, const String&);
    void setValue(const String& name, std::unique_ptr<Value>);
    void setObject(const String& name, std::unique_ptr<DictionaryValue>);
    void setArray(const String& name, std::unique_ptr<ListValue>);

    bool getBoolean(const String& name, bool* output) const;
    bool getInteger(const String& name, int* output) const;
    bool getDouble(const String& name, double* output) const;
    bool getString(const String& name, String* output) const;

    DictionaryValue* getObject(const String& name) const;
    ListValue* getArray(const String& name) const;
    Value* get(const String& name) const;
    Entry at(size_t index) const;

    bool booleanProperty(const String& name, bool defaultValue) const;
    int integerProperty(const String& name, int defaultValue) const;
    double doubleProperty(const String& name, double defaultValue) const;
    void remove(const String& name);

    ~DictionaryValue() override;

private:
    DictionaryValue();
    template<typename T>
    void set(const String& key, std::unique_ptr<T>& value)
    {
        DCHECK(value);
        bool isNew = m_data.find(key) == m_data.end();
        m_data[key] = std::move(value);
        if (isNew)
            m_order.push_back(key);
    }

    using Dictionary = protocol::HashMap<String, std::unique_ptr<Value>>;
    Dictionary m_data;
    std::vector<String> m_order;
};

class  ListValue : public Value {
public:
    static std::unique_ptr<ListValue> create()
    {
        return std::unique_ptr<ListValue>(new ListValue());
    }

    static ListValue* cast(Value* value)
    {
        if (!value || value->type() != TypeArray)
            return nullptr;
        return static_cast<ListValue*>(value);
    }

    static std::unique_ptr<ListValue> cast(std::unique_ptr<Value> value)
    {
        return std::unique_ptr<ListValue>(ListValue::cast(value.release()));
    }

    ~ListValue() override;

    void writeJSON(StringBuilder* output) const override;
    std::unique_ptr<Value> clone() const override;

    void pushValue(std::unique_ptr<Value>);

    Value* at(size_t index);
    size_t size() const { return m_data.size(); }

private:
    ListValue();
    std::vector<std::unique_ptr<Value>> m_data;
};

void escapeLatinStringForJSON(const uint8_t* str, unsigned len, StringBuilder* dst);
void escapeWideStringForJSON(const uint16_t* str, unsigned len, StringBuilder* dst);

} // namespace node
} // namespace inspector
} // namespace protocol

#endif // node_inspector_protocol_Values_h


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_Object_h
#define node_inspector_protocol_Object_h

//#include "ErrorSupport.h"
//#include "Forward.h"
//#include "Values.h"

namespace node {
namespace inspector {
namespace protocol {

class  Object {
public:
    static std::unique_ptr<Object> fromValue(protocol::Value*, ErrorSupport*);
    ~Object();

    std::unique_ptr<protocol::DictionaryValue> toValue() const;
    std::unique_ptr<Object> clone() const;
private:
    explicit Object(std::unique_ptr<protocol::DictionaryValue>);
    std::unique_ptr<protocol::DictionaryValue> m_object;
};

} // namespace node
} // namespace inspector
} // namespace protocol

#endif // !defined(node_inspector_protocol_Object_h)


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_ValueConversions_h
#define node_inspector_protocol_ValueConversions_h

//#include "ErrorSupport.h"
//#include "Forward.h"
//#include "Values.h"

namespace node {
namespace inspector {
namespace protocol {

template<typename T>
struct ValueConversions {
    static std::unique_ptr<T> fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        return T::fromValue(value, errors);
    }

    static std::unique_ptr<protocol::Value> toValue(T* value)
    {
        return value->toValue();
    }

    static std::unique_ptr<protocol::Value> toValue(const std::unique_ptr<T>& value)
    {
        return value->toValue();
    }
};

template<>
struct ValueConversions<bool> {
    static bool fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        bool result = false;
        bool success = value ? value->asBoolean(&result) : false;
        if (!success)
            errors->addError("boolean value expected");
        return result;
    }

    static std::unique_ptr<protocol::Value> toValue(bool value)
    {
        return FundamentalValue::create(value);
    }
};

template<>
struct ValueConversions<int> {
    static int fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        int result = 0;
        bool success = value ? value->asInteger(&result) : false;
        if (!success)
            errors->addError("integer value expected");
        return result;
    }

    static std::unique_ptr<protocol::Value> toValue(int value)
    {
        return FundamentalValue::create(value);
    }
};

template<>
struct ValueConversions<double> {
    static double fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        double result = 0;
        bool success = value ? value->asDouble(&result) : false;
        if (!success)
            errors->addError("double value expected");
        return result;
    }

    static std::unique_ptr<protocol::Value> toValue(double value)
    {
        return FundamentalValue::create(value);
    }
};

template<>
struct ValueConversions<String> {
    static String fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        String result;
        bool success = value ? value->asString(&result) : false;
        if (!success)
            errors->addError("string value expected");
        return result;
    }

    static std::unique_ptr<protocol::Value> toValue(const String& value)
    {
        return StringValue::create(value);
    }
};

template<>
struct ValueConversions<Value> {
    static std::unique_ptr<Value> fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        bool success = !!value;
        if (!success) {
            errors->addError("value expected");
            return nullptr;
        }
        return value->clone();
    }

    static std::unique_ptr<protocol::Value> toValue(Value* value)
    {
        return value->clone();
    }

    static std::unique_ptr<protocol::Value> toValue(const std::unique_ptr<Value>& value)
    {
        return value->clone();
    }
};

template<>
struct ValueConversions<DictionaryValue> {
    static std::unique_ptr<DictionaryValue> fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        bool success = value && value->type() == protocol::Value::TypeObject;
        if (!success)
            errors->addError("object expected");
        return DictionaryValue::cast(value->clone());
    }

    static std::unique_ptr<protocol::Value> toValue(DictionaryValue* value)
    {
        return value->clone();
    }

    static std::unique_ptr<protocol::Value> toValue(const std::unique_ptr<DictionaryValue>& value)
    {
        return value->clone();
    }
};

template<>
struct ValueConversions<ListValue> {
    static std::unique_ptr<ListValue> fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        bool success = value && value->type() == protocol::Value::TypeArray;
        if (!success)
            errors->addError("list expected");
        return ListValue::cast(value->clone());
    }

    static std::unique_ptr<protocol::Value> toValue(ListValue* value)
    {
        return value->clone();
    }

    static std::unique_ptr<protocol::Value> toValue(const std::unique_ptr<ListValue>& value)
    {
        return value->clone();
    }
};

} // namespace node
} // namespace inspector
} // namespace protocol

#endif // !defined(node_inspector_protocol_ValueConversions_h)


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_Maybe_h
#define node_inspector_protocol_Maybe_h

//#include "Forward.h"

namespace node {
namespace inspector {
namespace protocol {

template<typename T>
class Maybe {
public:
    Maybe() : m_value() { }
    Maybe(std::unique_ptr<T> value) : m_value(std::move(value)) { }
    Maybe(Maybe&& other) : m_value(std::move(other.m_value)) { }
    void operator=(std::unique_ptr<T> value) { m_value = std::move(value); }
    T* fromJust() const { DCHECK(m_value); return m_value.get(); }
    T* fromMaybe(T* defaultValue) const { return m_value ? m_value.get() : defaultValue; }
    bool isJust() const { return !!m_value; }
    std::unique_ptr<T> takeJust() { DCHECK(m_value); return std::move(m_value); }
private:
    std::unique_ptr<T> m_value;
};

template<typename T>
class MaybeBase {
public:
    MaybeBase() : m_isJust(false) { }
    MaybeBase(T value) : m_isJust(true), m_value(value) { }
    MaybeBase(MaybeBase&& other) : m_isJust(other.m_isJust), m_value(std::move(other.m_value)) { }
    void operator=(T value) { m_value = value; m_isJust = true; }
    T fromJust() const { DCHECK(m_isJust); return m_value; }
    T fromMaybe(const T& defaultValue) const { return m_isJust ? m_value : defaultValue; }
    bool isJust() const { return m_isJust; }
    T takeJust() { DCHECK(m_isJust); return m_value; }

protected:
    bool m_isJust;
    T m_value;
};

template<>
class Maybe<bool> : public MaybeBase<bool> {
public:
    Maybe() { }
    Maybe(bool value) : MaybeBase(value) { }
    Maybe(Maybe&& other) : MaybeBase(std::move(other)) { }
    using MaybeBase::operator=;
};

template<>
class Maybe<int> : public MaybeBase<int> {
public:
    Maybe() { }
    Maybe(int value) : MaybeBase(value) { }
    Maybe(Maybe&& other) : MaybeBase(std::move(other)) { }
    using MaybeBase::operator=;
};

template<>
class Maybe<double> : public MaybeBase<double> {
public:
    Maybe() { }
    Maybe(double value) : MaybeBase(value) { }
    Maybe(Maybe&& other) : MaybeBase(std::move(other)) { }
    using MaybeBase::operator=;
};

template<>
class Maybe<String> : public MaybeBase<String> {
public:
    Maybe() { }
    Maybe(const String& value) : MaybeBase(value) { }
    Maybe(Maybe&& other) : MaybeBase(std::move(other)) { }
    using MaybeBase::operator=;
};

} // namespace node
} // namespace inspector
} // namespace protocol

#endif // !defined(node_inspector_protocol_Maybe_h)


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_Array_h
#define node_inspector_protocol_Array_h

//#include "ErrorSupport.h"
//#include "Forward.h"
//#include "ValueConversions.h"
//#include "Values.h"

namespace node {
namespace inspector {
namespace protocol {

template<typename T>
class Array {
public:
    static std::unique_ptr<Array<T>> create()
    {
        return std::unique_ptr<Array<T>>(new Array<T>());
    }

    static std::unique_ptr<Array<T>> fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        protocol::ListValue* array = ListValue::cast(value);
        if (!array) {
            errors->addError("array expected");
            return nullptr;
        }
        std::unique_ptr<Array<T>> result(new Array<T>());
        errors->push();
        for (size_t i = 0; i < array->size(); ++i) {
            errors->setName(StringUtil::fromInteger(i));
            std::unique_ptr<T> item = ValueConversions<T>::fromValue(array->at(i), errors);
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

    std::unique_ptr<protocol::ListValue> toValue()
    {
        std::unique_ptr<protocol::ListValue> result = ListValue::create();
        for (auto& item : m_vector)
            result->pushValue(ValueConversions<T>::toValue(item));
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
        return std::unique_ptr<Array<T>>(new Array<T>());
    }

    static std::unique_ptr<Array<T>> fromValue(protocol::Value* value, ErrorSupport* errors)
    {
        protocol::ListValue* array = ListValue::cast(value);
        if (!array) {
            errors->addError("array expected");
            return nullptr;
        }
        errors->push();
        std::unique_ptr<Array<T>> result(new Array<T>());
        for (size_t i = 0; i < array->size(); ++i) {
            errors->setName(StringUtil::fromInteger(i));
            T item = ValueConversions<T>::fromValue(array->at(i), errors);
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

    std::unique_ptr<protocol::ListValue> toValue()
    {
        std::unique_ptr<protocol::ListValue> result = ListValue::create();
        for (auto& item : m_vector)
            result->pushValue(ValueConversions<T>::toValue(item));
        return result;
    }

private:
    std::vector<T> m_vector;
};

template<> class Array<String> : public ArrayBase<String> {};
template<> class Array<int> : public ArrayBase<int> {};
template<> class Array<double> : public ArrayBase<double> {};
template<> class Array<bool> : public ArrayBase<bool> {};

} // namespace node
} // namespace inspector
} // namespace protocol

#endif // !defined(node_inspector_protocol_Array_h)


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_DispatcherBase_h
#define node_inspector_protocol_DispatcherBase_h

//#include "Collections.h"
//#include "ErrorSupport.h"
//#include "Forward.h"
//#include "Values.h"

namespace node {
namespace inspector {
namespace protocol {

class WeakPtr;

class  DispatchResponse {
public:
    enum Status {
        kSuccess = 0,
        kError = 1,
        kFallThrough = 2,
        kAsync = 3
    };

    enum ErrorCode {
        kParseError = -32700,
        kInvalidRequest = -32600,
        kMethodNotFound = -32601,
        kInvalidParams = -32602,
        kInternalError = -32603,
        kServerError = -32000,
    };

    Status status() const { return m_status; }
    const String& errorMessage() const { return m_errorMessage; }
    ErrorCode errorCode() const { return m_errorCode; }
    bool isSuccess() const { return m_status == kSuccess; }

    static DispatchResponse OK();
    static DispatchResponse Error(const String&);
    static DispatchResponse InternalError();
    static DispatchResponse InvalidParams(const String&);
    static DispatchResponse FallThrough();

private:
    Status m_status;
    String m_errorMessage;
    ErrorCode m_errorCode;
};

class  DispatcherBase {
    PROTOCOL_DISALLOW_COPY(DispatcherBase);
public:
    static const char kInvalidParamsString[];
    class  WeakPtr {
    public:
        explicit WeakPtr(DispatcherBase*);
        ~WeakPtr();
        DispatcherBase* get() { return m_dispatcher; }
        void dispose() { m_dispatcher = nullptr; }

    private:
        DispatcherBase* m_dispatcher;
    };

    class  Callback {
    public:
        Callback(std::unique_ptr<WeakPtr> backendImpl, int callId, int callbackId);
        virtual ~Callback();
        void dispose();

    protected:
        void sendIfActive(std::unique_ptr<protocol::DictionaryValue> partialMessage, const DispatchResponse& response);
        void fallThroughIfActive();

    private:
        std::unique_ptr<WeakPtr> m_backendImpl;
        int m_callId;
        int m_callbackId;
    };

    explicit DispatcherBase(FrontendChannel*);
    virtual ~DispatcherBase();

    virtual DispatchResponse::Status dispatch(int callId, const String& method, std::unique_ptr<protocol::DictionaryValue> messageObject) = 0;

    void sendResponse(int callId, const DispatchResponse&, std::unique_ptr<protocol::DictionaryValue> result);
    void sendResponse(int callId, const DispatchResponse&);

    void reportProtocolError(int callId, DispatchResponse::ErrorCode, const String& errorMessage, ErrorSupport* errors);
    void clearFrontend();

    std::unique_ptr<WeakPtr> weakPtr();

    int nextCallbackId();
    void markFallThrough(int callbackId);
    bool lastCallbackFallThrough() { return m_lastCallbackFallThrough; }

private:
    FrontendChannel* m_frontendChannel;
    protocol::HashSet<WeakPtr*> m_weakPtrs;
    int m_lastCallbackId;
    bool m_lastCallbackFallThrough;
};

class  UberDispatcher {
    PROTOCOL_DISALLOW_COPY(UberDispatcher);
public:
    explicit UberDispatcher(FrontendChannel*);
    void registerBackend(const String& name, std::unique_ptr<protocol::DispatcherBase>);
    void setupRedirects(const HashMap<String, String>&);
    DispatchResponse::Status dispatch(std::unique_ptr<Value> message, int* callId = nullptr, String* method = nullptr);
    FrontendChannel* channel() { return m_frontendChannel; }
    bool fallThroughForNotFound() { return m_fallThroughForNotFound; }
    void setFallThroughForNotFound(bool);
    bool getCommandName(const String& message, String* method, std::unique_ptr<protocol::DictionaryValue>* parsedMessage);
    virtual ~UberDispatcher();

private:
    FrontendChannel* m_frontendChannel;
    bool m_fallThroughForNotFound;
    HashMap<String, String> m_redirects;
    protocol::HashMap<String, std::unique_ptr<protocol::DispatcherBase>> m_dispatchers;
};

class InternalResponse : public Serializable {
    PROTOCOL_DISALLOW_COPY(InternalResponse);
public:
    static std::unique_ptr<InternalResponse> createResponse(int callId, std::unique_ptr<Serializable> params);
    static std::unique_ptr<InternalResponse> createNotification(const String& notification, std::unique_ptr<Serializable> params = nullptr);

    String serialize() override;

    ~InternalResponse() override {}

private:
    InternalResponse(int callId, const String& notification, std::unique_ptr<Serializable> params);

    int m_callId;
    String m_notification;
    std::unique_ptr<Serializable> m_params;
};

class InternalRawNotification : public Serializable {
public:
    static std::unique_ptr<InternalRawNotification> create(const String& notification)
    {
        return std::unique_ptr<InternalRawNotification>(new InternalRawNotification(notification));
    }
    ~InternalRawNotification() override {}

    String serialize() override
    {
        return m_notification;
    }

private:
  explicit InternalRawNotification(const String& notification)
    : m_notification(notification)
  {
  }

  String m_notification;
};

} // namespace node
} // namespace inspector
} // namespace protocol

#endif // !defined(node_inspector_protocol_DispatcherBase_h)


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_Parser_h
#define node_inspector_protocol_Parser_h

//#include "Forward.h"
//#include "Values.h"

namespace node {
namespace inspector {
namespace protocol {

 std::unique_ptr<Value> parseJSONCharacters(const uint8_t*, unsigned);
 std::unique_ptr<Value> parseJSONCharacters(const uint16_t*, unsigned);

} // namespace node
} // namespace inspector
} // namespace protocol

#endif // !defined(node_inspector_protocol_Parser_h)
