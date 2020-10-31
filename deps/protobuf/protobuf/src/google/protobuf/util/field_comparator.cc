// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

// Author: ksroka@google.com (Krzysztof Sroka)

#include <google/protobuf/util/field_comparator.h>

#include <string>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/stubs/map_util.h>
#include <google/protobuf/stubs/mathlimits.h>
#include <google/protobuf/stubs/mathutil.h>

namespace google {
namespace protobuf {
namespace util {

FieldComparator::FieldComparator() {}
FieldComparator::~FieldComparator() {}

DefaultFieldComparator::DefaultFieldComparator()
    : float_comparison_(EXACT),
      treat_nan_as_equal_(false),
      has_default_tolerance_(false) {}

DefaultFieldComparator::~DefaultFieldComparator() {}

FieldComparator::ComparisonResult DefaultFieldComparator::Compare(
    const Message& message_1, const Message& message_2,
    const FieldDescriptor* field, int index_1, int index_2,
    const util::FieldContext* field_context) {
  const Reflection* reflection_1 = message_1.GetReflection();
  const Reflection* reflection_2 = message_2.GetReflection();

  switch (field->cpp_type()) {
#define COMPARE_FIELD(METHOD)                                                 \
  if (field->is_repeated()) {                                                 \
    return ResultFromBoolean(Compare##METHOD(                                 \
        *field, reflection_1->GetRepeated##METHOD(message_1, field, index_1), \
        reflection_2->GetRepeated##METHOD(message_2, field, index_2)));       \
  } else {                                                                    \
    return ResultFromBoolean(                                                 \
        Compare##METHOD(*field, reflection_1->Get##METHOD(message_1, field),  \
                        reflection_2->Get##METHOD(message_2, field)));        \
  }                                                                           \
  break;  // Make sure no fall-through is introduced.

    case FieldDescriptor::CPPTYPE_BOOL:
      COMPARE_FIELD(Bool);
    case FieldDescriptor::CPPTYPE_DOUBLE:
      COMPARE_FIELD(Double);
    case FieldDescriptor::CPPTYPE_ENUM:
      COMPARE_FIELD(Enum);
    case FieldDescriptor::CPPTYPE_FLOAT:
      COMPARE_FIELD(Float);
    case FieldDescriptor::CPPTYPE_INT32:
      COMPARE_FIELD(Int32);
    case FieldDescriptor::CPPTYPE_INT64:
      COMPARE_FIELD(Int64);
    case FieldDescriptor::CPPTYPE_STRING:
      if (field->is_repeated()) {
        // Allocate scratch strings to store the result if a conversion is
        // needed.
        std::string scratch1;
        std::string scratch2;
        return ResultFromBoolean(
            CompareString(*field,
                          reflection_1->GetRepeatedStringReference(
                              message_1, field, index_1, &scratch1),
                          reflection_2->GetRepeatedStringReference(
                              message_2, field, index_2, &scratch2)));
      } else {
        // Allocate scratch strings to store the result if a conversion is
        // needed.
        std::string scratch1;
        std::string scratch2;
        return ResultFromBoolean(CompareString(
            *field,
            reflection_1->GetStringReference(message_1, field, &scratch1),
            reflection_2->GetStringReference(message_2, field, &scratch2)));
      }
      break;
    case FieldDescriptor::CPPTYPE_UINT32:
      COMPARE_FIELD(UInt32);
    case FieldDescriptor::CPPTYPE_UINT64:
      COMPARE_FIELD(UInt64);

#undef COMPARE_FIELD

    case FieldDescriptor::CPPTYPE_MESSAGE:
      return RECURSE;

    default:
      GOOGLE_LOG(FATAL) << "No comparison code for field " << field->full_name()
                 << " of CppType = " << field->cpp_type();
      return DIFFERENT;
  }
}

bool DefaultFieldComparator::Compare(MessageDifferencer* differencer,
                                     const Message& message1,
                                     const Message& message2,
                                     const util::FieldContext* field_context) {
  return differencer->Compare(message1, message2,
                              field_context->parent_fields());
}

void DefaultFieldComparator::SetDefaultFractionAndMargin(double fraction,
                                                         double margin) {
  default_tolerance_ = Tolerance(fraction, margin);
  has_default_tolerance_ = true;
}

void DefaultFieldComparator::SetFractionAndMargin(const FieldDescriptor* field,
                                                  double fraction,
                                                  double margin) {
  GOOGLE_CHECK(FieldDescriptor::CPPTYPE_FLOAT == field->cpp_type() ||
        FieldDescriptor::CPPTYPE_DOUBLE == field->cpp_type())
      << "Field has to be float or double type. Field name is: "
      << field->full_name();
  map_tolerance_[field] = Tolerance(fraction, margin);
}

bool DefaultFieldComparator::CompareDouble(const FieldDescriptor& field,
                                           double value_1, double value_2) {
  return CompareDoubleOrFloat(field, value_1, value_2);
}

bool DefaultFieldComparator::CompareEnum(const FieldDescriptor& field,
                                         const EnumValueDescriptor* value_1,
                                         const EnumValueDescriptor* value_2) {
  return value_1->number() == value_2->number();
}

bool DefaultFieldComparator::CompareFloat(const FieldDescriptor& field,
                                          float value_1, float value_2) {
  return CompareDoubleOrFloat(field, value_1, value_2);
}

template <typename T>
bool DefaultFieldComparator::CompareDoubleOrFloat(const FieldDescriptor& field,
                                                  T value_1, T value_2) {
  if (value_1 == value_2) {
    // Covers +inf and -inf (which are not within margin or fraction of
    // themselves), and is a shortcut for finite values.
    return true;
  } else if (float_comparison_ == EXACT) {
    if (treat_nan_as_equal_ && MathLimits<T>::IsNaN(value_1) &&
        MathLimits<T>::IsNaN(value_2)) {
      return true;
    }
    return false;
  } else {
    if (treat_nan_as_equal_ && MathLimits<T>::IsNaN(value_1) &&
        MathLimits<T>::IsNaN(value_2)) {
      return true;
    }
    // float_comparison_ == APPROXIMATE covers two use cases.
    Tolerance* tolerance = FindOrNull(map_tolerance_, &field);
    if (tolerance == NULL && has_default_tolerance_) {
      tolerance = &default_tolerance_;
    }
    if (tolerance == NULL) {
      return MathUtil::AlmostEquals(value_1, value_2);
    } else {
      // Use user-provided fraction and margin. Since they are stored as
      // doubles, we explicitly cast them to types of values provided. This
      // is very likely to fail if provided values are not numeric.
      return MathUtil::WithinFractionOrMargin(
          value_1, value_2, static_cast<T>(tolerance->fraction),
          static_cast<T>(tolerance->margin));
    }
  }
}

FieldComparator::ComparisonResult DefaultFieldComparator::ResultFromBoolean(
    bool boolean_result) const {
  return boolean_result ? FieldComparator::SAME : FieldComparator::DIFFERENT;
}

}  // namespace util
}  // namespace protobuf
}  // namespace google
