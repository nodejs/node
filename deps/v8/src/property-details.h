// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_PROPERTY_DETAILS_H_
#define V8_PROPERTY_DETAILS_H_

#include "../include/v8.h"
#include "allocation.h"
#include "utils.h"

// Ecma-262 3rd 8.6.1
enum PropertyAttributes {
  NONE              = v8::None,
  READ_ONLY         = v8::ReadOnly,
  DONT_ENUM         = v8::DontEnum,
  DONT_DELETE       = v8::DontDelete,
  ABSENT            = 16  // Used in runtime to indicate a property is absent.
  // ABSENT can never be stored in or returned from a descriptor's attributes
  // bitfield.  It is only used as a return value meaning the attributes of
  // a non-existent property.
};


namespace v8 {
namespace internal {

class Smi;

// Type of properties.
// Order of properties is significant.
// Must fit in the BitField PropertyDetails::TypeField.
// A copy of this is in mirror-debugger.js.
enum PropertyType {
  NORMAL                    = 0,  // only in slow mode
  FIELD                     = 1,  // only in fast mode
  CONSTANT_FUNCTION         = 2,  // only in fast mode
  CALLBACKS                 = 3,
  HANDLER                   = 4,  // only in lookup results, not in descriptors
  INTERCEPTOR               = 5,  // only in lookup results, not in descriptors
  // All properties before MAP_TRANSITION are real.
  MAP_TRANSITION            = 6,  // only in fast mode
  ELEMENTS_TRANSITION       = 7,
  CONSTANT_TRANSITION       = 8,  // only in fast mode
  NULL_DESCRIPTOR           = 9,  // only in fast mode
  // There are no IC stubs for NULL_DESCRIPTORS. Therefore,
  // NULL_DESCRIPTOR can be used as the type flag for IC stubs for
  // nonexistent properties.
  NONEXISTENT = NULL_DESCRIPTOR
};


// PropertyDetails captures type and attributes for a property.
// They are used both in property dictionaries and instance descriptors.
class PropertyDetails BASE_EMBEDDED {
 public:
  PropertyDetails(PropertyAttributes attributes,
                  PropertyType type,
                  int index = 0) {
    ASSERT(TypeField::is_valid(type));
    ASSERT(AttributesField::is_valid(attributes));
    ASSERT(StorageField::is_valid(index));

    value_ = TypeField::encode(type)
        | AttributesField::encode(attributes)
        | StorageField::encode(index);

    ASSERT(type == this->type());
    ASSERT(attributes == this->attributes());
    ASSERT(index == this->index());
  }

  // Conversion for storing details as Object*.
  explicit inline PropertyDetails(Smi* smi);
  inline Smi* AsSmi();

  PropertyType type() { return TypeField::decode(value_); }

  PropertyAttributes attributes() { return AttributesField::decode(value_); }

  int index() { return StorageField::decode(value_); }

  inline PropertyDetails AsDeleted();

  static bool IsValidIndex(int index) {
    return StorageField::is_valid(index);
  }

  bool IsReadOnly() { return (attributes() & READ_ONLY) != 0; }
  bool IsDontDelete() { return (attributes() & DONT_DELETE) != 0; }
  bool IsDontEnum() { return (attributes() & DONT_ENUM) != 0; }
  bool IsDeleted() { return DeletedField::decode(value_) != 0;}

  // Bit fields in value_ (type, shift, size). Must be public so the
  // constants can be embedded in generated code.
  class TypeField:       public BitField<PropertyType,       0, 4> {};
  class AttributesField: public BitField<PropertyAttributes, 4, 3> {};
  class DeletedField:    public BitField<uint32_t,           7, 1> {};
  class StorageField:    public BitField<uint32_t,           8, 32-8> {};

  static const int kInitialIndex = 1;

 private:
  uint32_t value_;
};

} }  // namespace v8::internal

#endif  // V8_PROPERTY_DETAILS_H_
