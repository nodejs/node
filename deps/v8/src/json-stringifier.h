// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_JSON_STRINGIFIER_H_
#define V8_JSON_STRINGIFIER_H_

#include "src/objects.h"
#include "src/string-builder.h"

namespace v8 {
namespace internal {

class JsonStringifier BASE_EMBEDDED {
 public:
  explicit JsonStringifier(Isolate* isolate);

  ~JsonStringifier() { DeleteArray(gap_); }

  MUST_USE_RESULT MaybeHandle<Object> Stringify(Handle<Object> object,
                                                Handle<Object> replacer,
                                                Handle<Object> gap);

 private:
  enum Result { UNCHANGED, SUCCESS, EXCEPTION };

  bool InitializeReplacer(Handle<Object> replacer);
  bool InitializeGap(Handle<Object> gap);

  MUST_USE_RESULT MaybeHandle<Object> ApplyToJsonFunction(
      Handle<Object> object,
      Handle<Object> key);
  MUST_USE_RESULT MaybeHandle<Object> ApplyReplacerFunction(
      Handle<Object> value, Handle<Object> key, Handle<Object> initial_holder);

  // Entry point to serialize the object.
  INLINE(Result SerializeObject(Handle<Object> obj)) {
    return Serialize_<false>(obj, false, factory()->empty_string());
  }

  // Serialize an array element.
  // The index may serve as argument for the toJSON function.
  INLINE(Result SerializeElement(Isolate* isolate,
                                 Handle<Object> object,
                                 int i)) {
    return Serialize_<false>(object,
                             false,
                             Handle<Object>(Smi::FromInt(i), isolate));
  }

  // Serialize a object property.
  // The key may or may not be serialized depending on the property.
  // The key may also serve as argument for the toJSON function.
  INLINE(Result SerializeProperty(Handle<Object> object,
                                  bool deferred_comma,
                                  Handle<String> deferred_key)) {
    DCHECK(!deferred_key.is_null());
    return Serialize_<true>(object, deferred_comma, deferred_key);
  }

  template <bool deferred_string_key>
  Result Serialize_(Handle<Object> object, bool comma, Handle<Object> key);

  INLINE(void SerializeDeferredKey(bool deferred_comma,
                                   Handle<Object> deferred_key));

  Result SerializeSmi(Smi* object);

  Result SerializeDouble(double number);
  INLINE(Result SerializeHeapNumber(Handle<HeapNumber> object)) {
    return SerializeDouble(object->value());
  }

  Result SerializeJSValue(Handle<JSValue> object);

  INLINE(Result SerializeJSArray(Handle<JSArray> object));
  INLINE(Result SerializeJSObject(Handle<JSObject> object));

  Result SerializeJSProxy(Handle<JSProxy> object);
  Result SerializeJSReceiverSlow(Handle<JSReceiver> object);
  Result SerializeArrayLikeSlow(Handle<JSReceiver> object, uint32_t start,
                                uint32_t length);

  void SerializeString(Handle<String> object);

  template <typename SrcChar, typename DestChar>
  INLINE(static void SerializeStringUnchecked_(
      Vector<const SrcChar> src,
      IncrementalStringBuilder::NoExtend<DestChar>* dest));

  template <typename SrcChar, typename DestChar>
  INLINE(void SerializeString_(Handle<String> string));

  template <typename Char>
  INLINE(static bool DoNotEscape(Char c));

  INLINE(void NewLine());
  INLINE(void Indent() { indent_++; });
  INLINE(void Unindent() { indent_--; });
  INLINE(void Separator(bool first));

  Handle<JSReceiver> CurrentHolder(Handle<Object> value,
                                   Handle<Object> inital_holder);

  Result StackPush(Handle<Object> object);
  void StackPop();

  Factory* factory() { return isolate_->factory(); }

  Isolate* isolate_;
  IncrementalStringBuilder builder_;
  Handle<String> tojson_string_;
  Handle<JSArray> stack_;
  Handle<FixedArray> property_list_;
  Handle<JSReceiver> replacer_function_;
  uc16* gap_;
  int indent_;

  static const int kJsonEscapeTableEntrySize = 8;
  static const char* const JsonEscapeTable;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_JSON_STRINGIFIER_H_
