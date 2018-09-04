// This file is generated

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef node_inspector_protocol_NodeTracing_h
#define node_inspector_protocol_NodeTracing_h

#include "inspector/node_protocol/Protocol.h"
// For each imported domain we generate a ValueConversions struct instead of a full domain definition
// and include Domain::API version from there.

namespace node {
namespace inspector {
namespace protocol {
namespace NodeTracing {

// ------------- Forward and enum declarations.
class TraceConfig;
class DataCollectedNotification;
using TracingCompleteNotification = Object;

// ------------- Type and builder declarations.

class  TraceConfig : public Serializable{
    PROTOCOL_DISALLOW_COPY(TraceConfig);
public:
    static std::unique_ptr<TraceConfig> fromValue(protocol::Value* value, ErrorSupport* errors);

    ~TraceConfig() override { }

    struct  RecordModeEnum {
        static const char* RecordUntilFull;
        static const char* RecordContinuously;
        static const char* RecordAsMuchAsPossible;
    }; // RecordModeEnum

    bool hasRecordMode() { return m_recordMode.isJust(); }
    String getRecordMode(const String& defaultValue) { return m_recordMode.isJust() ? m_recordMode.fromJust() : defaultValue; }
    void setRecordMode(const String& value) { m_recordMode = value; }

    protocol::Array<String>* getIncludedCategories() { return m_includedCategories.get(); }
    void setIncludedCategories(std::unique_ptr<protocol::Array<String>> value) { m_includedCategories = std::move(value); }

    std::unique_ptr<protocol::DictionaryValue> toValue() const;
    String serialize() override { return toValue()->serialize(); }
    std::unique_ptr<TraceConfig> clone() const;

    template<int STATE>
    class TraceConfigBuilder {
    public:
        enum {
            NoFieldsSet = 0,
            IncludedCategoriesSet = 1 << 1,
            AllFieldsSet = (IncludedCategoriesSet | 0)};


        TraceConfigBuilder<STATE>& setRecordMode(const String& value)
        {
            m_result->setRecordMode(value);
            return *this;
        }

        TraceConfigBuilder<STATE | IncludedCategoriesSet>& setIncludedCategories(std::unique_ptr<protocol::Array<String>> value)
        {
            static_assert(!(STATE & IncludedCategoriesSet), "property includedCategories should not be set yet");
            m_result->setIncludedCategories(std::move(value));
            return castState<IncludedCategoriesSet>();
        }

        std::unique_ptr<TraceConfig> build()
        {
            static_assert(STATE == AllFieldsSet, "state should be AllFieldsSet");
            return std::move(m_result);
        }

    private:
        friend class TraceConfig;
        TraceConfigBuilder() : m_result(new TraceConfig()) { }

        template<int STEP> TraceConfigBuilder<STATE | STEP>& castState()
        {
            return *reinterpret_cast<TraceConfigBuilder<STATE | STEP>*>(this);
        }

        std::unique_ptr<protocol::NodeTracing::TraceConfig> m_result;
    };

    static TraceConfigBuilder<0> create()
    {
        return TraceConfigBuilder<0>();
    }

private:
    TraceConfig()
    {
    }

    Maybe<String> m_recordMode;
    std::unique_ptr<protocol::Array<String>> m_includedCategories;
};


class  DataCollectedNotification : public Serializable{
    PROTOCOL_DISALLOW_COPY(DataCollectedNotification);
public:
    static std::unique_ptr<DataCollectedNotification> fromValue(protocol::Value* value, ErrorSupport* errors);

    ~DataCollectedNotification() override { }

    protocol::Array<protocol::DictionaryValue>* getValue() { return m_value.get(); }
    void setValue(std::unique_ptr<protocol::Array<protocol::DictionaryValue>> value) { m_value = std::move(value); }

    std::unique_ptr<protocol::DictionaryValue> toValue() const;
    String serialize() override { return toValue()->serialize(); }
    std::unique_ptr<DataCollectedNotification> clone() const;

    template<int STATE>
    class DataCollectedNotificationBuilder {
    public:
        enum {
            NoFieldsSet = 0,
            ValueSet = 1 << 1,
            AllFieldsSet = (ValueSet | 0)};


        DataCollectedNotificationBuilder<STATE | ValueSet>& setValue(std::unique_ptr<protocol::Array<protocol::DictionaryValue>> value)
        {
            static_assert(!(STATE & ValueSet), "property value should not be set yet");
            m_result->setValue(std::move(value));
            return castState<ValueSet>();
        }

        std::unique_ptr<DataCollectedNotification> build()
        {
            static_assert(STATE == AllFieldsSet, "state should be AllFieldsSet");
            return std::move(m_result);
        }

    private:
        friend class DataCollectedNotification;
        DataCollectedNotificationBuilder() : m_result(new DataCollectedNotification()) { }

        template<int STEP> DataCollectedNotificationBuilder<STATE | STEP>& castState()
        {
            return *reinterpret_cast<DataCollectedNotificationBuilder<STATE | STEP>*>(this);
        }

        std::unique_ptr<protocol::NodeTracing::DataCollectedNotification> m_result;
    };

    static DataCollectedNotificationBuilder<0> create()
    {
        return DataCollectedNotificationBuilder<0>();
    }

private:
    DataCollectedNotification()
    {
    }

    std::unique_ptr<protocol::Array<protocol::DictionaryValue>> m_value;
};


// ------------- Backend interface.

class  Backend {
public:
    virtual ~Backend() { }

    virtual DispatchResponse getCategories(std::unique_ptr<protocol::Array<String>>* out_categories) = 0;
    virtual DispatchResponse start(std::unique_ptr<protocol::NodeTracing::TraceConfig> in_traceConfig) = 0;
    virtual DispatchResponse stop() = 0;

    virtual DispatchResponse disable()
    {
        return DispatchResponse::OK();
    }
};

// ------------- Frontend interface.

class  Frontend {
public:
    explicit Frontend(FrontendChannel* frontendChannel) : m_frontendChannel(frontendChannel) { }
    void dataCollected(std::unique_ptr<protocol::Array<protocol::DictionaryValue>> value);
    void tracingComplete();

    void flush();
    void sendRawNotification(const String&);
private:
    FrontendChannel* m_frontendChannel;
};

// ------------- Dispatcher.

class  Dispatcher {
public:
    static void wire(UberDispatcher*, Backend*);

private:
    Dispatcher() { }
};

// ------------- Metainfo.

class  Metainfo {
public:
    using BackendClass = Backend;
    using FrontendClass = Frontend;
    using DispatcherClass = Dispatcher;
    static const char domainName[];
    static const char commandPrefix[];
    static const char version[];
};

} // namespace NodeTracing
} // namespace node
} // namespace inspector
} // namespace protocol

#endif // !defined(node_inspector_protocol_NodeTracing_h)
