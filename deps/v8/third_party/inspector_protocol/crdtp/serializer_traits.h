// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRDTP_SERIALIZER_TRAITS_H_
#define V8_CRDTP_SERIALIZER_TRAITS_H_

#include <memory>
#include <string>
#include <vector>
#include "cbor.h"
#include "glue.h"
#include "span.h"

namespace v8_crdtp {
// =============================================================================
// SerializerTraits - Encodes field values of protocol objects in CBOR.
// =============================================================================
//
// A family of serialization functions which are used by FieldSerializerTraits
// (below) to encode field values in CBOR. Conceptually, it's this:
//
// Serialize(bool value, std::vector<uint8_t>* out);
// Serialize(int32_t value, std::vector<uint8_t>* out);
// Serialize(double value, std::vector<uint8_t>* out);
// ...
//
// However, if this was to use straight-forward overloading, implicit
// type conversions would lead to ambiguity - e.g., a bool could be
// represented as an int32_t, but it should really be encoded as a bool.
// The template parameterized / specialized structs accomplish this.
//
// SerializerTraits<bool>::Serialize(bool value, std::vector<uint8_t>* out);
// SerializerTraits<int32>::Serialize(int32_t value, std::vector<uint8_t>* out);
// SerializerTraits<double>::Serialize(double value, std::vector<uint8_t>* out);
template <typename T>
struct SerializerTraits {
  // |Serializable| (defined in serializable.h) already knows how to serialize
  // to CBOR, so we can just delegate. This covers domain specific types,
  // protocol::Binary, etc.
  // However, we use duck-typing here, because Exported, which is part of the V8
  // headers also comes with AppendSerialized, and logically it's the same type,
  // but it lives in a different namespace (v8_inspector::protocol::Exported).
  template <
      typename LikeSerializable,
      typename std::enable_if_t<std::is_member_pointer<decltype(
                                    &LikeSerializable::AppendSerialized)>{},
                                int> = 0>
  static void Serialize(const LikeSerializable& value,
                        std::vector<uint8_t>* out) {
    value.AppendSerialized(out);
  }
};

// This covers std::string, which is assumed to be UTF-8.
// The two other string implementations that are used in the protocol bindings:
// - WTF::String, for which the SerializerTraits specialization is located
//   in third_party/blink/renderer/core/inspector/v8-inspector-string.h.
// - v8_inspector::String16, implemented in v8/src/inspector/string-16.h
//   along with its SerializerTraits specialization.
template <>
struct SerializerTraits<std::string> {
  static void Serialize(const std::string& str, std::vector<uint8_t>* out) {
    cbor::EncodeString8(SpanFrom(str), out);
  }
};

template <>
struct SerializerTraits<bool> {
  static void Serialize(bool value, std::vector<uint8_t>* out) {
    out->push_back(value ? cbor::EncodeTrue() : cbor::EncodeFalse());
  }
};

template <>
struct SerializerTraits<int32_t> {
  static void Serialize(int32_t value, std::vector<uint8_t>* out) {
    cbor::EncodeInt32(value, out);
  }
};

template <>
struct SerializerTraits<double> {
  static void Serialize(double value, std::vector<uint8_t>* out) {
    cbor::EncodeDouble(value, out);
  }
};

template <typename T>
struct SerializerTraits<std::vector<T>> {
  static void Serialize(const std::vector<T>& value,
                        std::vector<uint8_t>* out) {
    out->push_back(cbor::EncodeIndefiniteLengthArrayStart());
    for (const T& element : value)
      SerializerTraits<T>::Serialize(element, out);
    out->push_back(cbor::EncodeStop());
  }
};

template <typename T>
struct SerializerTraits<std::unique_ptr<T>> {
  static void Serialize(const std::unique_ptr<T>& value,
                        std::vector<uint8_t>* out) {
    SerializerTraits<T>::Serialize(*value, out);
  }
};

// =============================================================================
// FieldSerializerTraits - Encodes fields of protocol objects in CBOR
// =============================================================================
//
// The generated code (see TypeBuilder_cpp.template) invokes SerializeField,
// which then instantiates the FieldSerializerTraits to emit the appropriate
// existence checks / dereference for the field value. This avoids emitting
// the field name if the value for an optional field isn't set.
template <typename T>
struct FieldSerializerTraits {
  static void Serialize(span<uint8_t> field_name,
                        const T& field_value,
                        std::vector<uint8_t>* out) {
    cbor::EncodeString8(field_name, out);
    SerializerTraits<T>::Serialize(field_value, out);
  }
};

template <typename T>
struct FieldSerializerTraits<glue::detail::PtrMaybe<T>> {
  static void Serialize(span<uint8_t> field_name,
                        const glue::detail::PtrMaybe<T>& field_value,
                        std::vector<uint8_t>* out) {
    if (!field_value.isJust())
      return;
    cbor::EncodeString8(field_name, out);
    SerializerTraits<T>::Serialize(*field_value.fromJust(), out);
  }
};

template <typename T>
struct FieldSerializerTraits<glue::detail::ValueMaybe<T>> {
  static void Serialize(span<uint8_t> field_name,
                        const glue::detail::ValueMaybe<T>& field_value,
                        std::vector<uint8_t>* out) {
    if (!field_value.isJust())
      return;
    cbor::EncodeString8(field_name, out);
    SerializerTraits<T>::Serialize(field_value.fromJust(), out);
  }
};

template <typename T>
void SerializeField(span<uint8_t> field_name,
                    const T& field_value,
                    std::vector<uint8_t>* out) {
  FieldSerializerTraits<T>::Serialize(field_name, field_value, out);
}
}  // namespace v8_crdtp

#endif  // V8_CRDTP_SERIALIZER_TRAITS_H_
