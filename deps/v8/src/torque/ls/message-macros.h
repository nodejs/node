// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_LS_MESSAGE_MACROS_H_
#define V8_TORQUE_LS_MESSAGE_MACROS_H_

namespace v8 {
namespace internal {
namespace torque {
namespace ls {

#define JSON_STRING_ACCESSORS(name)                \
  inline const std::string& name() const {         \
    return object().at(#name).ToString();          \
  }                                                \
  inline void set_##name(const std::string& str) { \
    object()[#name] = JsonValue::From(str);        \
  }                                                \
  inline bool has_##name() const {                 \
    return object().find(#name) != object().end(); \
  }

#define JSON_BOOL_ACCESSORS(name)                                  \
  inline bool name() const { return object().at(#name).ToBool(); } \
  inline void set_##name(bool b) { object()[#name] = JsonValue::From(b); }

#define JSON_INT_ACCESSORS(name)                                    \
  inline int name() const { return object().at(#name).ToNumber(); } \
  inline void set_##name(int n) {                                   \
    object()[#name] = JsonValue::From(static_cast<double>(n));      \
  }

#define JSON_OBJECT_ACCESSORS(type, name) \
  inline type name() { return GetObject<type>(#name); }

#define JSON_DYNAMIC_OBJECT_ACCESSORS(name) \
  template <class T>                        \
  inline T name() {                         \
    return GetObject<T>(#name);             \
  }

#define JSON_ARRAY_OBJECT_ACCESSORS(type, name)                               \
  inline type add_##name() {                                                  \
    JsonObject& new_element = AddObjectElementToArrayProperty(#name);         \
    return type(new_element);                                                 \
  }                                                                           \
  inline std::size_t name##_size() { return GetArrayProperty(#name).size(); } \
  inline type name(size_t idx) {                                              \
    CHECK(idx < name##_size());                                               \
    return type(GetArrayProperty(#name)[idx].ToObject());                     \
  }

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_LS_MESSAGE_MACROS_H_
