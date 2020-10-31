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

// Author: jschorr@google.com (Joseph Schorr)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This file defines static methods and classes for comparing Protocol
// Messages.
//
// Aug. 2008: Added Unknown Fields Comparison for messages.
// Aug. 2009: Added different options to compare repeated fields.
// Apr. 2010: Moved field comparison to FieldComparator.

#ifndef GOOGLE_PROTOBUF_UTIL_MESSAGE_DIFFERENCER_H__
#define GOOGLE_PROTOBUF_UTIL_MESSAGE_DIFFERENCER_H__

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <google/protobuf/descriptor.h>  // FieldDescriptor
#include <google/protobuf/message.h>     // Message
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/util/field_comparator.h>

// Always include as last one, otherwise it can break compilation
#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

class DynamicMessageFactory;
class FieldDescriptor;

namespace io {
class ZeroCopyOutputStream;
class Printer;
}  // namespace io

namespace util {

class DefaultFieldComparator;
class FieldContext;  // declared below MessageDifferencer

// Defines a collection of field descriptors.
// In case of internal google codebase we are using absl::FixedArray instead
// of vector. It significantly speeds up proto comparison (by ~30%) by
// reducing the number of malloc/free operations
typedef std::vector<const FieldDescriptor*> FieldDescriptorArray;

// A basic differencer that can be used to determine
// the differences between two specified Protocol Messages. If any differences
// are found, the Compare method will return false, and any differencer reporter
// specified via ReportDifferencesTo will have its reporting methods called (see
// below for implementation of the report). Based off of the original
// ProtocolDifferencer implementation in //net/proto/protocol-differencer.h
// (Thanks Todd!).
//
// MessageDifferencer REQUIRES that compared messages be the same type, defined
// as messages that share the same descriptor.  If not, the behavior of this
// class is undefined.
//
// People disagree on what MessageDifferencer should do when asked to compare
// messages with different descriptors.  Some people think it should always
// return false.  Others expect it to try to look for similar fields and
// compare them anyway -- especially if the descriptors happen to be identical.
// If we chose either of these behaviors, some set of people would find it
// surprising, and could end up writing code expecting the other behavior
// without realizing their error.  Therefore, we forbid that usage.
//
// This class is implemented based on the proto2 reflection. The performance
// should be good enough for normal usages. However, for places where the
// performance is extremely sensitive, there are several alternatives:
// - Comparing serialized string
// Downside: false negatives (there are messages that are the same but their
// serialized strings are different).
// - Equals code generator by compiler plugin (net/proto2/contrib/equals_plugin)
// Downside: more generated code; maintenance overhead for the additional rule
// (must be in sync with the original proto_library).
//
// Note on handling of google.protobuf.Any: MessageDifferencer automatically
// unpacks Any::value into a Message and compares its individual fields.
// Messages encoded in a repeated Any cannot be compared using TreatAsMap.
//
//
// Note on thread-safety: MessageDifferencer is *not* thread-safe. You need to
// guard it with a lock to use the same MessageDifferencer instance from
// multiple threads. Note that it's fine to call static comparison methods
// (like MessageDifferencer::Equals) concurrently.
class PROTOBUF_EXPORT MessageDifferencer {
 public:
  // Determines whether the supplied messages are equal. Equality is defined as
  // all fields within the two messages being set to the same value. Primitive
  // fields and strings are compared by value while embedded messages/groups
  // are compared as if via a recursive call. Use Compare() with IgnoreField()
  // if some fields should be ignored in the comparison. Use Compare() with
  // TreatAsSet() if there are repeated fields where ordering does not matter.
  //
  // This method REQUIRES that the two messages have the same
  // Descriptor (message1.GetDescriptor() == message2.GetDescriptor()).
  static bool Equals(const Message& message1, const Message& message2);

  // Determines whether the supplied messages are equivalent. Equivalency is
  // defined as all fields within the two messages having the same value. This
  // differs from the Equals method above in that fields with default values
  // are considered set to said value automatically. For details on how default
  // values are defined for each field type, see:
  // https://developers.google.com/protocol-buffers/docs/proto?csw=1#optional.
  // Also, Equivalent() ignores unknown fields. Use IgnoreField() and Compare()
  // if some fields should be ignored in the comparison.
  //
  // This method REQUIRES that the two messages have the same
  // Descriptor (message1.GetDescriptor() == message2.GetDescriptor()).
  static bool Equivalent(const Message& message1, const Message& message2);

  // Determines whether the supplied messages are approximately equal.
  // Approximate equality is defined as all fields within the two messages
  // being approximately equal.  Primitive (non-float) fields and strings are
  // compared by value, floats are compared using MathUtil::AlmostEquals() and
  // embedded messages/groups are compared as if via a recursive call. Use
  // IgnoreField() and Compare() if some fields should be ignored in the
  // comparison.
  //
  // This method REQUIRES that the two messages have the same
  // Descriptor (message1.GetDescriptor() == message2.GetDescriptor()).
  static bool ApproximatelyEquals(const Message& message1,
                                  const Message& message2);

  // Determines whether the supplied messages are approximately equivalent.
  // Approximate equivalency is defined as all fields within the two messages
  // being approximately equivalent. As in
  // MessageDifferencer::ApproximatelyEquals, primitive (non-float) fields and
  // strings are compared by value, floats are compared using
  // MathUtil::AlmostEquals() and embedded messages/groups are compared as if
  // via a recursive call. However, fields with default values are considered
  // set to said value, as per MessageDiffencer::Equivalent. Use IgnoreField()
  // and Compare() if some fields should be ignored in the comparison.
  //
  // This method REQUIRES that the two messages have the same
  // Descriptor (message1.GetDescriptor() == message2.GetDescriptor()).
  static bool ApproximatelyEquivalent(const Message& message1,
                                      const Message& message2);

  // Identifies an individual field in a message instance.  Used for field_path,
  // below.
  struct SpecificField {
    // For known fields, "field" is filled in and "unknown_field_number" is -1.
    // For unknown fields, "field" is NULL, "unknown_field_number" is the field
    // number, and "unknown_field_type" is its type.
    const FieldDescriptor* field;
    int unknown_field_number;
    UnknownField::Type unknown_field_type;

    // If this a repeated field, "index" is the index within it.  For unknown
    // fields, this is the index of the field among all unknown fields of the
    // same field number and type.
    int index;

    // If "field" is a repeated field which is being treated as a map or
    // a set (see TreatAsMap() and TreatAsSet(), below), new_index indicates
    // the index the position to which the element has moved.  If the element
    // has not moved, "new_index" will have the same value as "index".
    int new_index;

    // For unknown fields, these are the pointers to the UnknownFieldSet
    // containing the unknown fields. In certain cases (e.g. proto1's
    // MessageSet, or nested groups of unknown fields), these may differ from
    // the messages' internal UnknownFieldSets.
    const UnknownFieldSet* unknown_field_set1;
    const UnknownFieldSet* unknown_field_set2;

    // For unknown fields, these are the index of the field within the
    // UnknownFieldSets. One or the other will be -1 when
    // reporting an addition or deletion.
    int unknown_field_index1;
    int unknown_field_index2;

    SpecificField()
        : field(NULL),
          unknown_field_number(-1),
          index(-1),
          new_index(-1),
          unknown_field_set1(NULL),
          unknown_field_set2(NULL),
          unknown_field_index1(-1),
          unknown_field_index2(-1) {}
  };

  // Abstract base class from which all MessageDifferencer
  // reporters derive. The five Report* methods below will be called when
  // a field has been added, deleted, modified, moved, or matched. The third
  // argument is a vector of FieldDescriptor pointers which describes the chain
  // of fields that was taken to find the current field. For example, for a
  // field found in an embedded message, the vector will contain two
  // FieldDescriptors. The first will be the field of the embedded message
  // itself and the second will be the actual field in the embedded message
  // that was added/deleted/modified.
  class PROTOBUF_EXPORT Reporter {
   public:
    Reporter();
    virtual ~Reporter();

    // Reports that a field has been added into Message2.
    virtual void ReportAdded(const Message& message1, const Message& message2,
                             const std::vector<SpecificField>& field_path) = 0;

    // Reports that a field has been deleted from Message1.
    virtual void ReportDeleted(
        const Message& message1, const Message& message2,
        const std::vector<SpecificField>& field_path) = 0;

    // Reports that the value of a field has been modified.
    virtual void ReportModified(
        const Message& message1, const Message& message2,
        const std::vector<SpecificField>& field_path) = 0;

    // Reports that a repeated field has been moved to another location.  This
    // only applies when using TreatAsSet or TreatAsMap()  -- see below. Also
    // note that for any given field, ReportModified and ReportMoved are
    // mutually exclusive. If a field has been both moved and modified, then
    // only ReportModified will be called.
    virtual void ReportMoved(
        const Message& /* message1 */, const Message& /* message2 */,
        const std::vector<SpecificField>& /* field_path */) {}

    // Reports that two fields match. Useful for doing side-by-side diffs.
    // This function is mutually exclusive with ReportModified and ReportMoved.
    // Note that you must call set_report_matches(true) before calling Compare
    // to make use of this function.
    virtual void ReportMatched(
        const Message& /* message1 */, const Message& /* message2 */,
        const std::vector<SpecificField>& /* field_path */) {}

    // Reports that two fields would have been compared, but the
    // comparison has been skipped because the field was marked as
    // 'ignored' using IgnoreField().  This function is mutually
    // exclusive with all the other Report() functions.
    //
    // The contract of ReportIgnored is slightly different than the
    // other Report() functions, in that |field_path.back().index| is
    // always equal to -1, even if the last field is repeated. This is
    // because while the other Report() functions indicate where in a
    // repeated field the action (Addition, Deletion, etc...)
    // happened, when a repeated field is 'ignored', the differencer
    // simply calls ReportIgnored on the repeated field as a whole and
    // moves on without looking at its individual elements.
    //
    // Furthermore, ReportIgnored() does not indicate whether the
    // fields were in fact equal or not, as Compare() does not inspect
    // these fields at all. It is up to the Reporter to decide whether
    // the fields are equal or not (perhaps with a second call to
    // Compare()), if it cares.
    virtual void ReportIgnored(
        const Message& /* message1 */, const Message& /* message2 */,
        const std::vector<SpecificField>& /* field_path */) {}

    // Report that an unknown field is ignored. (see comment above).
    // Note this is a different function since the last SpecificField in field
    // path has a null field.  This could break existing Reporter.
    virtual void ReportUnknownFieldIgnored(
        const Message& /* message1 */, const Message& /* message2 */,
        const std::vector<SpecificField>& /* field_path */) {}

   private:
    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Reporter);
  };

  // MapKeyComparator is used to determine if two elements have the same key
  // when comparing elements of a repeated field as a map.
  class PROTOBUF_EXPORT MapKeyComparator {
   public:
    MapKeyComparator();
    virtual ~MapKeyComparator();

    virtual bool IsMatch(
        const Message& /* message1 */, const Message& /* message2 */,
        const std::vector<SpecificField>& /* parent_fields */) const {
      GOOGLE_CHECK(false) << "IsMatch() is not implemented.";
      return false;
    }

   private:
    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MapKeyComparator);
  };

  // Abstract base class from which all IgnoreCriteria derive.
  // By adding IgnoreCriteria more complex ignore logic can be implemented.
  // IgnoreCriteria are registed with AddIgnoreCriteria. For each compared
  // field IsIgnored is called on each added IgnoreCriteria until one returns
  // true or all return false.
  // IsIgnored is called for fields where at least one side has a value.
  class PROTOBUF_EXPORT IgnoreCriteria {
   public:
    IgnoreCriteria();
    virtual ~IgnoreCriteria();

    // Returns true if the field should be ignored.
    virtual bool IsIgnored(
        const Message& /* message1 */, const Message& /* message2 */,
        const FieldDescriptor* /* field */,
        const std::vector<SpecificField>& /* parent_fields */) = 0;

    // Returns true if the unknown field should be ignored.
    // Note: This will be called for unknown fields as well in which case
    //       field.field will be null.
    virtual bool IsUnknownFieldIgnored(
        const Message& /* message1 */, const Message& /* message2 */,
        const SpecificField& /* field */,
        const std::vector<SpecificField>& /* parent_fields */) {
      return false;
    }
  };

  // To add a Reporter, construct default here, then use ReportDifferencesTo or
  // ReportDifferencesToString.
  explicit MessageDifferencer();

  ~MessageDifferencer();

  enum MessageFieldComparison {
    EQUAL,       // Fields must be present in both messages
                 // for the messages to be considered the same.
    EQUIVALENT,  // Fields with default values are considered set
                 // for comparison purposes even if not explicitly
                 // set in the messages themselves.  Unknown fields
                 // are ignored.
  };

  enum Scope {
    FULL,    // All fields of both messages are considered in the comparison.
    PARTIAL  // Only fields present in the first message are considered; fields
             // set only in the second message will be skipped during
             // comparison.
  };

  // DEPRECATED. Use FieldComparator::FloatComparison instead.
  enum FloatComparison {
    EXACT,       // Floats and doubles are compared exactly.
    APPROXIMATE  // Floats and doubles are compared using the
                 // MathUtil::AlmostEquals method.
  };

  enum RepeatedFieldComparison {
    AS_LIST,  // Repeated fields are compared in order.  Differing values at
              // the same index are reported using ReportModified().  If the
              // repeated fields have different numbers of elements, the
              // unpaired elements are reported using ReportAdded() or
              // ReportDeleted().
    AS_SET,   // Treat all the repeated fields as sets.
              // See TreatAsSet(), as below.
    AS_SMART_LIST,  // Similar to AS_SET, but preserve the order and find the
                    // longest matching sequence from the first matching
                    // element. To use an optimal solution, call
                    // SetMatchIndicesForSmartListCallback() to pass it in.
    AS_SMART_SET,   // Similar to AS_SET, but match elements with fewest diffs.
  };

  // The elements of the given repeated field will be treated as a set for
  // diffing purposes, so different orderings of the same elements will be
  // considered equal.  Elements which are present on both sides of the
  // comparison but which have changed position will be reported with
  // ReportMoved().  Elements which only exist on one side or the other are
  // reported with ReportAdded() and ReportDeleted() regardless of their
  // positions.  ReportModified() is never used for this repeated field.  If
  // the only differences between the compared messages is that some fields
  // have been moved, then the comparison returns true.
  //
  // Note that despite the name of this method, this is really
  // comparison as multisets: if one side of the comparison has a duplicate
  // in the repeated field but the other side doesn't, this will count as
  // a mismatch.
  //
  // If the scope of comparison is set to PARTIAL, then in addition to what's
  // above, extra values added to repeated fields of the second message will
  // not cause the comparison to fail.
  //
  // Note that set comparison is currently O(k * n^2) (where n is the total
  // number of elements, and k is the average size of each element). In theory
  // it could be made O(n * k) with a more complex hashing implementation. Feel
  // free to contribute one if the current implementation is too slow for you.
  // If partial matching is also enabled, the time complexity will be O(k * n^2
  // + n^3) in which n^3 is the time complexity of the maximum matching
  // algorithm.
  //
  // REQUIRES:  field->is_repeated() and field not registered with TreatAsList
  void TreatAsSet(const FieldDescriptor* field);
  void TreatAsSmartSet(const FieldDescriptor* field);

  // The elements of the given repeated field will be treated as a list for
  // diffing purposes, so different orderings of the same elements will NOT be
  // considered equal.
  //
  // REQUIRED: field->is_repeated() and field not registered with TreatAsSet
  void TreatAsList(const FieldDescriptor* field);
  // Note that the complexity is similar to treating as SET.
  void TreatAsSmartList(const FieldDescriptor* field);

  // The elements of the given repeated field will be treated as a map for
  // diffing purposes, with |key| being the map key.  Thus, elements with the
  // same key will be compared even if they do not appear at the same index.
  // Differences are reported similarly to TreatAsSet(), except that
  // ReportModified() is used to report elements with the same key but
  // different values.  Note that if an element is both moved and modified,
  // only ReportModified() will be called.  As with TreatAsSet, if the only
  // differences between the compared messages is that some fields have been
  // moved, then the comparison returns true. See TreatAsSet for notes on
  // performance.
  //
  // REQUIRES:  field->is_repeated()
  // REQUIRES:  field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE
  // REQUIRES:  key->containing_type() == field->message_type()
  void TreatAsMap(const FieldDescriptor* field, const FieldDescriptor* key);
  // Same as TreatAsMap except that this method will use multiple fields as
  // the key in comparison. All specified fields in 'key_fields' should be
  // present in the compared elements. Two elements will be treated as having
  // the same key iff they have the same value for every specified field. There
  // are two steps in the comparison process. The first one is key matching.
  // Every element from one message will be compared to every element from
  // the other message. Only fields in 'key_fields' are compared in this step
  // to decide if two elements have the same key. The second step is value
  // comparison. Those pairs of elements with the same key (with equal value
  // for every field in 'key_fields') will be compared in this step.
  // Time complexity of the first step is O(s * m * n ^ 2) where s is the
  // average size of the fields specified in 'key_fields', m is the number of
  // fields in 'key_fields' and n is the number of elements. If partial
  // matching is enabled, an extra O(n^3) will be incured by the maximum
  // matching algorithm. The second step is O(k * n) where k is the average
  // size of each element.
  void TreatAsMapWithMultipleFieldsAsKey(
      const FieldDescriptor* field,
      const std::vector<const FieldDescriptor*>& key_fields);
  // Same as TreatAsMapWithMultipleFieldsAsKey, except that each of the field
  // do not necessarily need to be a direct subfield. Each element in
  // key_field_paths indicate a path from the message being compared, listing
  // successive subfield to reach the key field.
  //
  // REQUIRES:
  //   for key_field_path in key_field_paths:
  //     key_field_path[0]->containing_type() == field->message_type()
  //     for i in [0, key_field_path.size() - 1):
  //       key_field_path[i+1]->containing_type() ==
  //           key_field_path[i]->message_type()
  //       key_field_path[i]->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE
  //       !key_field_path[i]->is_repeated()
  void TreatAsMapWithMultipleFieldPathsAsKey(
      const FieldDescriptor* field,
      const std::vector<std::vector<const FieldDescriptor*> >& key_field_paths);

  // Uses a custom MapKeyComparator to determine if two elements have the same
  // key when comparing a repeated field as a map.
  // The caller is responsible to delete the key_comparator.
  // This method varies from TreatAsMapWithMultipleFieldsAsKey only in the
  // first key matching step. Rather than comparing some specified fields, it
  // will invoke the IsMatch method of the given 'key_comparator' to decide if
  // two elements have the same key.
  void TreatAsMapUsingKeyComparator(const FieldDescriptor* field,
                                    const MapKeyComparator* key_comparator);

  // Initiates and returns a new instance of MultipleFieldsMapKeyComparator.
  MapKeyComparator* CreateMultipleFieldsMapKeyComparator(
      const std::vector<std::vector<const FieldDescriptor*> >& key_field_paths);

  // Add a custom ignore criteria that is evaluated in addition to the
  // ignored fields added with IgnoreField.
  // Takes ownership of ignore_criteria.
  void AddIgnoreCriteria(IgnoreCriteria* ignore_criteria);

  // Indicates that any field with the given descriptor should be
  // ignored for the purposes of comparing two messages. This applies
  // to fields nested in the message structure as well as top level
  // ones. When the MessageDifferencer encounters an ignored field,
  // ReportIgnored is called on the reporter, if one is specified.
  //
  // The only place where the field's 'ignored' status is not applied is when
  // it is being used as a key in a field passed to TreatAsMap or is one of
  // the fields passed to TreatAsMapWithMultipleFieldsAsKey.
  // In this case it is compared in key matching but after that it's ignored
  // in value comparison.
  void IgnoreField(const FieldDescriptor* field);

  // Sets the field comparator used to determine differences between protocol
  // buffer fields. By default it's set to a DefaultFieldComparator instance.
  // MessageDifferencer doesn't take ownership over the passed object.
  // Note that this method must be called before Compare for the comparator to
  // be used.
  void set_field_comparator(FieldComparator* comparator);

  // DEPRECATED. Pass a DefaultFieldComparator instance instead.
  // Sets the fraction and margin for the float comparison of a given field.
  // Uses MathUtil::WithinFractionOrMargin to compare the values.
  // NOTE: this method does nothing if differencer's field comparator has been
  //       set to a custom object.
  //
  // REQUIRES: field->cpp_type == FieldDescriptor::CPPTYPE_DOUBLE or
  //           field->cpp_type == FieldDescriptor::CPPTYPE_FLOAT
  // REQUIRES: float_comparison_ == APPROXIMATE
  void SetFractionAndMargin(const FieldDescriptor* field, double fraction,
                            double margin);

  // Sets the type of comparison (as defined in the MessageFieldComparison
  // enumeration above) that is used by this differencer when determining how
  // to compare fields in messages.
  void set_message_field_comparison(MessageFieldComparison comparison);

  // Tells the differencer whether or not to report matches. This method must
  // be called before Compare. The default for a new differencer is false.
  void set_report_matches(bool report_matches) {
    report_matches_ = report_matches;
  }

  // Tells the differencer whether or not to report moves (in a set or map
  // repeated field). This method must be called before Compare. The default for
  // a new differencer is true.
  void set_report_moves(bool report_moves) { report_moves_ = report_moves; }

  // Tells the differencer whether or not to report ignored values. This method
  // must be called before Compare. The default for a new differencer is true.
  void set_report_ignores(bool report_ignores) {
    report_ignores_ = report_ignores;
  }

  // Sets the scope of the comparison (as defined in the Scope enumeration
  // above) that is used by this differencer when determining which fields to
  // compare between the messages.
  void set_scope(Scope scope);

  // Returns the current scope used by this differencer.
  Scope scope();

  // DEPRECATED. Pass a DefaultFieldComparator instance instead.
  // Sets the type of comparison (as defined in the FloatComparison enumeration
  // above) that is used by this differencer when comparing float (and double)
  // fields in messages.
  // NOTE: this method does nothing if differencer's field comparator has been
  //       set to a custom object.
  void set_float_comparison(FloatComparison comparison);

  // Sets the type of comparison for repeated field (as defined in the
  // RepeatedFieldComparison enumeration above) that is used by this
  // differencer when compare repeated fields in messages.
  void set_repeated_field_comparison(RepeatedFieldComparison comparison);

  // Compares the two specified messages, returning true if they are the same,
  // false otherwise. If this method returns false, any changes between the
  // two messages will be reported if a Reporter was specified via
  // ReportDifferencesTo (see also ReportDifferencesToString).
  //
  // This method REQUIRES that the two messages have the same
  // Descriptor (message1.GetDescriptor() == message2.GetDescriptor()).
  bool Compare(const Message& message1, const Message& message2);

  // Same as above, except comparing only the list of fields specified by the
  // two vectors of FieldDescriptors.
  bool CompareWithFields(
      const Message& message1, const Message& message2,
      const std::vector<const FieldDescriptor*>& message1_fields,
      const std::vector<const FieldDescriptor*>& message2_fields);

  // Automatically creates a reporter that will output the differences
  // found (if any) to the specified output string pointer. Note that this
  // method must be called before Compare.
  void ReportDifferencesToString(std::string* output);

  // Tells the MessageDifferencer to report differences via the specified
  // reporter. Note that this method must be called before Compare for
  // the reporter to be used. It is the responsibility of the caller to delete
  // this object.
  // If the provided pointer equals NULL, the MessageDifferencer stops reporting
  // differences to any previously set reporters or output strings.
  void ReportDifferencesTo(Reporter* reporter);

  // An implementation of the MessageDifferencer Reporter that outputs
  // any differences found in human-readable form to the supplied
  // ZeroCopyOutputStream or Printer. If a printer is used, the delimiter
  // *must* be '$'.
  //
  // WARNING: this reporter does not necessarily flush its output until it is
  // destroyed. As a result, it is not safe to assume the output is valid or
  // complete until after you destroy the reporter. For example, if you use a
  // StreamReporter to write to a StringOutputStream, the target string may
  // contain uninitialized data until the reporter is destroyed.
  class PROTOBUF_EXPORT StreamReporter : public Reporter {
   public:
    explicit StreamReporter(io::ZeroCopyOutputStream* output);
    explicit StreamReporter(io::Printer* printer);  // delimiter '$'
    ~StreamReporter() override;

    // When set to true, the stream reporter will also output aggregates nodes
    // (i.e. messages and groups) whose subfields have been modified. When
    // false, will only report the individual subfields. Defaults to false.
    void set_report_modified_aggregates(bool report) {
      report_modified_aggregates_ = report;
    }

    // The following are implementations of the methods described above.

    void ReportAdded(const Message& message1, const Message& message2,
                     const std::vector<SpecificField>& field_path) override;

    void ReportDeleted(const Message& message1, const Message& message2,
                       const std::vector<SpecificField>& field_path) override;

    void ReportModified(const Message& message1, const Message& message2,
                        const std::vector<SpecificField>& field_path) override;

    void ReportMoved(const Message& message1, const Message& message2,
                     const std::vector<SpecificField>& field_path) override;

    void ReportMatched(const Message& message1, const Message& message2,
                       const std::vector<SpecificField>& field_path) override;

    void ReportIgnored(const Message& message1, const Message& message2,
                       const std::vector<SpecificField>& field_path) override;

    void ReportUnknownFieldIgnored(
        const Message& message1, const Message& message2,
        const std::vector<SpecificField>& field_path) override;

   protected:
    // Prints the specified path of fields to the buffer.  message is used to
    // print map keys.
    virtual void PrintPath(const std::vector<SpecificField>& field_path,
                           bool left_side, const Message& message);

    // Prints the specified path of fields to the buffer.
    virtual void PrintPath(const std::vector<SpecificField>& field_path,
                           bool left_side);

    // Prints the value of fields to the buffer.  left_side is true if the
    // given message is from the left side of the comparison, false if it
    // was the right.  This is relevant only to decide whether to follow
    // unknown_field_index1 or unknown_field_index2 when an unknown field
    // is encountered in field_path.
    virtual void PrintValue(const Message& message,
                            const std::vector<SpecificField>& field_path,
                            bool left_side);

    // Prints the specified path of unknown fields to the buffer.
    virtual void PrintUnknownFieldValue(const UnknownField* unknown_field);

    // Just print a string
    void Print(const std::string& str);

   private:
    io::Printer* printer_;
    bool delete_printer_;
    bool report_modified_aggregates_;

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(StreamReporter);
  };

 private:
  friend class DefaultFieldComparator;

  // A MapKeyComparator to be used in TreatAsMapUsingKeyComparator.
  // Implementation of this class needs to do field value comparison which
  // relies on some private methods of MessageDifferencer. That's why this
  // class is declared as a nested class of MessageDifferencer.
  class MultipleFieldsMapKeyComparator;

  // A MapKeyComparator for use with map_entries.
  class PROTOBUF_EXPORT MapEntryKeyComparator : public MapKeyComparator {
   public:
    explicit MapEntryKeyComparator(MessageDifferencer* message_differencer);
    bool IsMatch(
        const Message& message1, const Message& message2,
        const std::vector<SpecificField>& parent_fields) const override;

   private:
    MessageDifferencer* message_differencer_;
  };

  // Returns true if field1's number() is less than field2's.
  static bool FieldBefore(const FieldDescriptor* field1,
                          const FieldDescriptor* field2);

  // Retrieve all the set fields, including extensions.
  FieldDescriptorArray RetrieveFields(const Message& message,
                                      bool base_message);

  // Combine the two lists of fields into the combined_fields output vector.
  // All fields present in both lists will always be included in the combined
  // list.  Fields only present in one of the lists will only appear in the
  // combined list if the corresponding fields_scope option is set to FULL.
  FieldDescriptorArray CombineFields(const FieldDescriptorArray& fields1,
                                     Scope fields1_scope,
                                     const FieldDescriptorArray& fields2,
                                     Scope fields2_scope);

  // Internal version of the Compare method which performs the actual
  // comparison. The parent_fields vector is a vector containing field
  // descriptors of all fields accessed to get to this comparison operation
  // (i.e. if the current message is an embedded message, the parent_fields
  // vector will contain the field that has this embedded message).
  bool Compare(const Message& message1, const Message& message2,
               std::vector<SpecificField>* parent_fields);

  // Compares all the unknown fields in two messages.
  bool CompareUnknownFields(const Message& message1, const Message& message2,
                            const UnknownFieldSet&, const UnknownFieldSet&,
                            std::vector<SpecificField>* parent_fields);

  // Compares the specified messages for the requested field lists. The field
  // lists are modified depending on comparison settings, and then passed to
  // CompareWithFieldsInternal.
  bool CompareRequestedFieldsUsingSettings(
      const Message& message1, const Message& message2,
      const FieldDescriptorArray& message1_fields,
      const FieldDescriptorArray& message2_fields,
      std::vector<SpecificField>* parent_fields);

  // Compares the specified messages with the specified field lists.
  bool CompareWithFieldsInternal(const Message& message1,
                                 const Message& message2,
                                 const FieldDescriptorArray& message1_fields,
                                 const FieldDescriptorArray& message2_fields,
                                 std::vector<SpecificField>* parent_fields);

  // Compares the repeated fields, and report the error.
  bool CompareRepeatedField(const Message& message1, const Message& message2,
                            const FieldDescriptor* field,
                            std::vector<SpecificField>* parent_fields);

  // Shorthand for CompareFieldValueUsingParentFields with NULL parent_fields.
  bool CompareFieldValue(const Message& message1, const Message& message2,
                         const FieldDescriptor* field, int index1, int index2);

  // Compares the specified field on the two messages, returning
  // true if they are the same, false otherwise. For repeated fields,
  // this method only compares the value in the specified index. This method
  // uses Compare functions to recurse into submessages.
  // The parent_fields vector is used in calls to a Reporter instance calls.
  // It can be NULL, in which case the MessageDifferencer will create new
  // list of parent messages if it needs to recursively compare the given field.
  // To avoid confusing users you should not set it to NULL unless you modified
  // Reporter to handle the change of parent_fields correctly.
  bool CompareFieldValueUsingParentFields(
      const Message& message1, const Message& message2,
      const FieldDescriptor* field, int index1, int index2,
      std::vector<SpecificField>* parent_fields);

  // Compares the specified field on the two messages, returning comparison
  // result, as returned by appropriate FieldComparator.
  FieldComparator::ComparisonResult GetFieldComparisonResult(
      const Message& message1, const Message& message2,
      const FieldDescriptor* field, int index1, int index2,
      const FieldContext* field_context);

  // Check if the two elements in the repeated field are match to each other.
  // if the key_comprator is NULL, this function returns true when the two
  // elements are equal.
  bool IsMatch(const FieldDescriptor* repeated_field,
               const MapKeyComparator* key_comparator, const Message* message1,
               const Message* message2,
               const std::vector<SpecificField>& parent_fields,
               Reporter* reporter, int index1, int index2);

  // Returns true when this repeated field has been configured to be treated
  // as a Set / SmartSet / SmartList.
  bool IsTreatedAsSet(const FieldDescriptor* field);
  bool IsTreatedAsSmartSet(const FieldDescriptor* field);

  bool IsTreatedAsSmartList(const FieldDescriptor* field);
  // When treating as SMART_LIST, it uses MatchIndicesPostProcessorForSmartList
  // by default to find the longest matching sequence from the first matching
  // element. The callback takes two vectors showing the matching indices from
  // the other vector, where -1 means an unmatch.
  void SetMatchIndicesForSmartListCallback(
      std::function<void(std::vector<int>*, std::vector<int>*)> callback);

  // Returns true when this repeated field is to be compared as a subset, ie.
  // has been configured to be treated as a set or map and scope is set to
  // PARTIAL.
  bool IsTreatedAsSubset(const FieldDescriptor* field);

  // Returns true if this field is to be ignored when this
  // MessageDifferencer compares messages.
  bool IsIgnored(const Message& message1, const Message& message2,
                 const FieldDescriptor* field,
                 const std::vector<SpecificField>& parent_fields);

  // Returns true if this unknown field is to be ignored when this
  // MessageDifferencer compares messages.
  bool IsUnknownFieldIgnored(const Message& message1, const Message& message2,
                             const SpecificField& field,
                             const std::vector<SpecificField>& parent_fields);

  // Returns MapKeyComparator* when this field has been configured to be treated
  // as a map or its is_map() return true.  If not, returns NULL.
  const MapKeyComparator* GetMapKeyComparator(
      const FieldDescriptor* field) const;

  // Attempts to match indices of a repeated field, so that the contained values
  // match. Clears output vectors and sets their values to indices of paired
  // messages, ie. if message1[0] matches message2[1], then match_list1[0] == 1
  // and match_list2[1] == 0. The unmatched indices are indicated by -1.
  // Assumes the repeated field is not treated as a simple list.
  // This method returns false if the match failed. However, it doesn't mean
  // that the comparison succeeds when this method returns true (you need to
  // double-check in this case).
  bool MatchRepeatedFieldIndices(
      const Message& message1, const Message& message2,
      const FieldDescriptor* repeated_field,
      const MapKeyComparator* key_comparator,
      const std::vector<SpecificField>& parent_fields,
      std::vector<int>* match_list1, std::vector<int>* match_list2);

  // If "any" is of type google.protobuf.Any, extract its payload using
  // DynamicMessageFactory and store in "data".
  bool UnpackAny(const Message& any, std::unique_ptr<Message>* data);

  // Checks if index is equal to new_index in all the specific fields.
  static bool CheckPathChanged(const std::vector<SpecificField>& parent_fields);

  // CHECKs that the given repeated field can be compared according to
  // new_comparison.
  void CheckRepeatedFieldComparisons(
      const FieldDescriptor* field,
      const RepeatedFieldComparison& new_comparison);

  // Defines a map between field descriptors and their MapKeyComparators.
  // Used for repeated fields when they are configured as TreatAsMap.
  typedef std::map<const FieldDescriptor*, const MapKeyComparator*>
      FieldKeyComparatorMap;

  // Defines a set to store field descriptors.  Used for repeated fields when
  // they are configured as TreatAsSet.
  typedef std::set<const FieldDescriptor*> FieldSet;
  typedef std::map<const FieldDescriptor*, RepeatedFieldComparison> FieldMap;

  Reporter* reporter_;
  DefaultFieldComparator default_field_comparator_;
  FieldComparator* field_comparator_;
  MessageFieldComparison message_field_comparison_;
  Scope scope_;
  RepeatedFieldComparison repeated_field_comparison_;

  FieldMap repeated_field_comparisons_;
  // Keeps track of MapKeyComparators that are created within
  // MessageDifferencer. These MapKeyComparators should be deleted
  // before MessageDifferencer is destroyed.
  // When TreatAsMap or TreatAsMapWithMultipleFieldsAsKey is called, we don't
  // store the supplied FieldDescriptors directly. Instead, a new
  // MapKeyComparator is created for comparison purpose.
  std::vector<MapKeyComparator*> owned_key_comparators_;
  FieldKeyComparatorMap map_field_key_comparator_;
  MapEntryKeyComparator map_entry_key_comparator_;
  std::vector<IgnoreCriteria*> ignore_criteria_;
  // Reused multiple times in RetrieveFields to avoid extra allocations
  std::vector<const FieldDescriptor*> tmp_message_fields_;

  FieldSet ignored_fields_;

  bool report_matches_;
  bool report_moves_;
  bool report_ignores_;

  std::string* output_string_;

  // Callback to post-process the matched indices to support SMART_LIST.
  std::function<void(std::vector<int>*, std::vector<int>*)>
      match_indices_for_smart_list_callback_;

  std::unique_ptr<DynamicMessageFactory> dynamic_message_factory_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MessageDifferencer);
};

// This class provides extra information to the FieldComparator::Compare
// function.
class PROTOBUF_EXPORT FieldContext {
 public:
  explicit FieldContext(
      std::vector<MessageDifferencer::SpecificField>* parent_fields)
      : parent_fields_(parent_fields) {}

  std::vector<MessageDifferencer::SpecificField>* parent_fields() const {
    return parent_fields_;
  }

 private:
  std::vector<MessageDifferencer::SpecificField>* parent_fields_;
};

}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_MESSAGE_DIFFERENCER_H__
