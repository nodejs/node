// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_VALUE_MIRROR_H_
#define V8_INSPECTOR_VALUE_MIRROR_H_

#include <memory>

#include "include/v8-inspector.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/string-16.h"

namespace v8_inspector {

class ValueMirror;
enum class WrapMode;

struct PrivatePropertyMirror {
  String16 name;
  std::unique_ptr<ValueMirror> value;
  std::unique_ptr<ValueMirror> getter;
  std::unique_ptr<ValueMirror> setter;
};

struct InternalPropertyMirror {
  String16 name;
  std::unique_ptr<ValueMirror> value;
};

struct PropertyMirror {
  String16 name;
  bool writable;
  bool configurable;
  bool enumerable;
  bool isOwn;
  bool isIndex;
  std::unique_ptr<ValueMirror> value;
  std::unique_ptr<ValueMirror> getter;
  std::unique_ptr<ValueMirror> setter;
  std::unique_ptr<ValueMirror> symbol;
  std::unique_ptr<ValueMirror> exception;
};

class ValueMirror {
 public:
  virtual ~ValueMirror();

  static std::unique_ptr<ValueMirror> create(v8::Local<v8::Context> context,
                                             v8::Local<v8::Value> value);
  virtual protocol::Response buildRemoteObject(
      v8::Local<v8::Context> context, WrapMode mode,
      std::unique_ptr<protocol::Runtime::RemoteObject>* result) const = 0;
  virtual void buildPropertyPreview(
      v8::Local<v8::Context> context, const String16& name,
      std::unique_ptr<protocol::Runtime::PropertyPreview>*) const {}
  virtual void buildObjectPreview(
      v8::Local<v8::Context> context, bool generatePreviewForTable,
      int* nameLimit, int* indexLimit,
      std::unique_ptr<protocol::Runtime::ObjectPreview>*) const {}
  virtual void buildEntryPreview(
      v8::Local<v8::Context> context, int* nameLimit, int* indexLimit,
      std::unique_ptr<protocol::Runtime::ObjectPreview>*) const {}
  virtual v8::Local<v8::Value> v8Value() const = 0;

  class PropertyAccumulator {
   public:
    virtual ~PropertyAccumulator() = default;
    virtual bool Add(PropertyMirror mirror) = 0;
  };
  static bool getProperties(v8::Local<v8::Context> context,
                            v8::Local<v8::Object> object, bool ownProperties,
                            bool accessorPropertiesOnly,
                            PropertyAccumulator* accumulator);
  static void getInternalProperties(
      v8::Local<v8::Context> context, v8::Local<v8::Object> object,
      std::vector<InternalPropertyMirror>* mirrors);
  static std::vector<PrivatePropertyMirror> getPrivateProperties(
      v8::Local<v8::Context> context, v8::Local<v8::Object> object);
};

protocol::Response toProtocolValue(v8::Local<v8::Context> context,
                                   v8::Local<v8::Value> value, int maxDepth,
                                   std::unique_ptr<protocol::Value>* result);
protocol::Response arrayToProtocolValue(
    v8::Local<v8::Context> context, v8::Local<v8::Array> array, int maxDepth,
    std::unique_ptr<protocol::ListValue>* result);
protocol::Response objectToProtocolValue(
    v8::Local<v8::Context> context, v8::Local<v8::Object> object, int maxDepth,
    std::unique_ptr<protocol::DictionaryValue>* result);

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_VALUE_MIRROR_H_
