// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TOOLS_V8WINDBG_SRC_V8_DEBUG_HELPER_INTEROP_H_
#define V8_TOOLS_V8WINDBG_SRC_V8_DEBUG_HELPER_INTEROP_H_

#include <wrl/client.h>

#include <DbgModel.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace WRL = Microsoft::WRL;

constexpr char kObject[] = "v8::internal::Object";
constexpr char16_t kObjectU[] = u"v8::internal::Object";
constexpr char kTaggedValue[] = "v8::internal::TaggedValue";
constexpr char16_t kTaggedValueU[] = u"v8::internal::TaggedValue";

enum class PropertyType {
  kPointer = 0,
  kArray = 1,
  kStruct = 2,
  kStructArray = kArray | kStruct,
};

struct StructField {
  StructField(std::u16string field_name, std::u16string type_name,
              std::string uncompressed_type_name, uint64_t address,
              uint8_t num_bits, uint8_t shift_bits);
  ~StructField();
  StructField(const StructField&);
  StructField(StructField&&);
  StructField& operator=(const StructField&);
  StructField& operator=(StructField&&);

  std::u16string name;

  // Statically-determined type, such as from .tq definition. This type should
  // be treated as if it were used in the v8::internal namespace; that is, type
  // "X::Y" can mean any of the following, in order of decreasing preference:
  // - v8::internal::X::Y
  // - v8::X::Y
  // - X::Y
  std::u16string type_name;

  // In some cases, |type_name| may be a simple type representing a compressed
  // pointer such as v8::internal::TaggedValue. In those cases,
  // |uncompressed_type_name| will contain the type of the object when
  // decompressed. Otherwise, |uncompressed_type_name| will match |type_name|.
  // In any case, it is safe to pass the |uncompressed_type_name| value as the
  // type_hint on a subsequent call to GetObjectProperties.
  std::string uncompressed_type_name;

  // Offset, in bytes, from beginning of struct.
  uint64_t offset;

  // The number of bits that are present, if this value is a bitfield. Zero
  // indicates that this value is not a bitfield (the full value is stored).
  uint8_t num_bits;

  // The number of bits by which this value has been left-shifted for storage as
  // a bitfield.
  uint8_t shift_bits;
};

struct Property {
  Property(std::u16string property_name, std::u16string type_name,
           std::string uncompressed_type_name, uint64_t address,
           size_t item_size);
  ~Property();
  Property(const Property&);
  Property(Property&&);
  Property& operator=(const Property&);
  Property& operator=(Property&&);

  std::u16string name;
  PropertyType type;

  // Statically-determined type, such as from .tq definition. Can be an empty
  // string if this property is itself a Torque-defined struct; in that case use
  // |fields| instead. This type should be treated as if it were used in the
  // v8::internal namespace; that is, type "X::Y" can mean any of the following,
  // in order of decreasing preference:
  // - v8::internal::X::Y
  // - v8::X::Y
  // - X::Y
  std::u16string type_name;

  // In some cases, |type_name| may be a simple type representing a compressed
  // pointer such as v8::internal::TaggedValue. In those cases,
  // |uncompressed_type_name| will contain the type of the object when
  // decompressed. Otherwise, |uncompressed_type_name| will match |type_name|.
  // In any case, it is safe to pass the |uncompressed_type_name| value as the
  // type_hint on a subsequent call to GetObjectProperties.
  std::string uncompressed_type_name;

  // The address where the property value can be found in the debuggee's address
  // space, or the address of the first value for an array.
  uint64_t addr_value;

  // Size of each array item, if this property is an array.
  size_t item_size;

  // Number of array items, if this property is an array.
  size_t length = 0;

  // Fields within this property, if this property is a struct, or fields within
  // each array element, if this property is a struct array.
  std::vector<StructField> fields;
};

struct V8HeapObject {
  V8HeapObject();
  ~V8HeapObject();
  V8HeapObject(const V8HeapObject&);
  V8HeapObject(V8HeapObject&&);
  V8HeapObject& operator=(const V8HeapObject&);
  V8HeapObject& operator=(V8HeapObject&&);
  std::u16string friendly_name;  // String to print in single-line description.
  std::vector<Property> properties;
};

V8HeapObject GetHeapObject(WRL::ComPtr<IDebugHostContext> sp_context,
                           uint64_t address, uint64_t referring_pointer,
                           const char* type_name, bool is_compressed);

// Expand a compressed pointer from 32 bits to the format that
// GetObjectProperties expects for compressed pointers.
inline uint64_t ExpandCompressedPointer(uint32_t ptr) { return ptr; }

std::vector<std::u16string> ListObjectClasses();

const char* BitsetName(uint64_t payload);

std::vector<Property> GetStackFrame(WRL::ComPtr<IDebugHostContext> sp_context,
                                    uint64_t frame_pointer);

#endif  // V8_TOOLS_V8WINDBG_SRC_V8_DEBUG_HELPER_INTEROP_H_
