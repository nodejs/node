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

// Defines classes for field comparison.

#ifndef GOOGLE_PROTOBUF_UTIL_FIELD_COMPARATOR_H__
#define GOOGLE_PROTOBUF_UTIL_FIELD_COMPARATOR_H__

#include <map>
#include <string>
#include <vector>

#include <google/protobuf/stubs/common.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

class Message;
class EnumValueDescriptor;
class FieldDescriptor;

namespace util {

class FieldContext;
class MessageDifferencer;

// Base class specifying the interface for comparing protocol buffer fields.
// Regular users should consider using or subclassing DefaultFieldComparator
// rather than this interface.
// Currently, this does not support comparing unknown fields.
class PROTOBUF_EXPORT FieldComparator {
 public:
  FieldComparator();
  virtual ~FieldComparator();

  enum ComparisonResult {
    SAME,       // Compared fields are equal. In case of comparing submessages,
                // user should not recursively compare their contents.
    DIFFERENT,  // Compared fields are different. In case of comparing
                // submessages, user should not recursively compare their
                // contents.
    RECURSE,    // Compared submessages need to be compared recursively.
                // FieldComparator does not specify the semantics of recursive
                // comparison. This value should not be returned for simple
                // values.
  };

  // Compares the values of a field in two protocol buffer messages.
  // Returns SAME or DIFFERENT for simple values, and SAME, DIFFERENT or RECURSE
  // for submessages. Returning RECURSE for fields not being submessages is
  // illegal.
  // In case the given FieldDescriptor points to a repeated field, the indices
  // need to be valid. Otherwise they should be ignored.
  //
  // FieldContext contains information about the specific instances of the
  // fields being compared, versus FieldDescriptor which only contains general
  // type information about the fields.
  virtual ComparisonResult Compare(const Message& message_1,
                                   const Message& message_2,
                                   const FieldDescriptor* field, int index_1,
                                   int index_2,
                                   const util::FieldContext* field_context) = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(FieldComparator);
};

// Basic implementation of FieldComparator.  Supports three modes of floating
// point value comparison: exact, approximate using MathUtil::AlmostEqual
// method, and arbitrarily precise using MathUtil::WithinFractionOrMargin.
class PROTOBUF_EXPORT DefaultFieldComparator : public FieldComparator {
 public:
  enum FloatComparison {
    EXACT,        // Floats and doubles are compared exactly.
    APPROXIMATE,  // Floats and doubles are compared using the
                  // MathUtil::AlmostEqual method or
                  // MathUtil::WithinFractionOrMargin method.
    // TODO(ksroka): Introduce third value to differentiate uses of AlmostEqual
    //               and WithinFractionOrMargin.
  };

  // Creates new comparator with float comparison set to EXACT.
  DefaultFieldComparator();

  ~DefaultFieldComparator() override;

  ComparisonResult Compare(const Message& message_1, const Message& message_2,
                           const FieldDescriptor* field, int index_1,
                           int index_2,
                           const util::FieldContext* field_context) override;

  void set_float_comparison(FloatComparison float_comparison) {
    float_comparison_ = float_comparison;
  }

  FloatComparison float_comparison() const { return float_comparison_; }

  // Set whether the FieldComparator shall treat floats or doubles that are both
  // NaN as equal (treat_nan_as_equal = true) or as different
  // (treat_nan_as_equal = false). Default is treating NaNs always as different.
  void set_treat_nan_as_equal(bool treat_nan_as_equal) {
    treat_nan_as_equal_ = treat_nan_as_equal;
  }

  bool treat_nan_as_equal() const { return treat_nan_as_equal_; }

  // Sets the fraction and margin for the float comparison of a given field.
  // Uses MathUtil::WithinFractionOrMargin to compare the values.
  //
  // REQUIRES: field->cpp_type == FieldDescriptor::CPPTYPE_DOUBLE or
  //           field->cpp_type == FieldDescriptor::CPPTYPE_FLOAT
  // REQUIRES: float_comparison_ == APPROXIMATE
  void SetFractionAndMargin(const FieldDescriptor* field, double fraction,
                            double margin);

  // Sets the fraction and margin for the float comparison of all float and
  // double fields, unless a field has been given a specific setting via
  // SetFractionAndMargin() above.
  // Uses MathUtil::WithinFractionOrMargin to compare the values.
  //
  // REQUIRES: float_comparison_ == APPROXIMATE
  void SetDefaultFractionAndMargin(double fraction, double margin);

 protected:
  // Compare using the provided message_differencer. For example, a subclass can
  // use this method to compare some field in a certain way using the same
  // message_differencer instance and the field context.
  bool Compare(MessageDifferencer* differencer, const Message& message1,
               const Message& message2,
               const util::FieldContext* field_context);

 private:
  // Defines the tolerance for floating point comparison (fraction and margin).
  struct Tolerance {
    double fraction;
    double margin;
    Tolerance() : fraction(0.0), margin(0.0) {}
    Tolerance(double f, double m) : fraction(f), margin(m) {}
  };

  // Defines the map to store the tolerances for floating point comparison.
  typedef std::map<const FieldDescriptor*, Tolerance> ToleranceMap;

  // The following methods get executed when CompareFields is called for the
  // basic types (instead of submessages). They return true on success. One
  // can use ResultFromBoolean() to convert that boolean to a ComparisonResult
  // value.
  bool CompareBool(const FieldDescriptor& /* unused */, bool value_1,
                   bool value_2) {
    return value_1 == value_2;
  }

  // Uses CompareDoubleOrFloat, a helper function used by both CompareDouble and
  // CompareFloat.
  bool CompareDouble(const FieldDescriptor& field, double value_1,
                     double value_2);

  bool CompareEnum(const FieldDescriptor& field,
                   const EnumValueDescriptor* value_1,
                   const EnumValueDescriptor* value_2);

  // Uses CompareDoubleOrFloat, a helper function used by both CompareDouble and
  // CompareFloat.
  bool CompareFloat(const FieldDescriptor& field, float value_1, float value_2);

  bool CompareInt32(const FieldDescriptor& /* unused */, int32 value_1,
                    int32 value_2) {
    return value_1 == value_2;
  }

  bool CompareInt64(const FieldDescriptor& /* unused */, int64 value_1,
                    int64 value_2) {
    return value_1 == value_2;
  }

  bool CompareString(const FieldDescriptor& /* unused */,
                     const std::string& value_1, const std::string& value_2) {
    return value_1 == value_2;
  }

  bool CompareUInt32(const FieldDescriptor& /* unused */, uint32 value_1,
                     uint32 value_2) {
    return value_1 == value_2;
  }

  bool CompareUInt64(const FieldDescriptor& /* unused */, uint64 value_1,
                     uint64 value_2) {
    return value_1 == value_2;
  }

  // This function is used by CompareDouble and CompareFloat to avoid code
  // duplication. There are no checks done against types of the values passed,
  // but it's likely to fail if passed non-numeric arguments.
  template <typename T>
  bool CompareDoubleOrFloat(const FieldDescriptor& field, T value_1, T value_2);

  // Returns FieldComparator::SAME if boolean_result is true and
  // FieldComparator::DIFFERENT otherwise.
  ComparisonResult ResultFromBoolean(bool boolean_result) const;

  FloatComparison float_comparison_;

  // If true, floats and doubles that are both NaN are considered to be
  // equal. Otherwise, two floats or doubles that are NaN are considered to be
  // different.
  bool treat_nan_as_equal_;

  // True iff default_tolerance_ has been explicitly set.
  //
  // If false, then the default tolerance for floats and doubles is that which
  // is used by MathUtil::AlmostEquals().
  bool has_default_tolerance_;

  // Default float/double tolerance. Only meaningful if
  // has_default_tolerance_ == true.
  Tolerance default_tolerance_;

  // Field-specific float/double tolerances, which override any default for
  // those particular fields.
  ToleranceMap map_tolerance_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(DefaultFieldComparator);
};

}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_FIELD_COMPARATOR_H__
