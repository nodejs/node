/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_DB_TYPED_COLUMN_H_
#define SRC_TRACE_PROCESSOR_DB_TYPED_COLUMN_H_

#include "src/trace_processor/db/column.h"
#include "src/trace_processor/db/typed_column_internal.h"

namespace perfetto {
namespace trace_processor {

// TypedColumn<T>
//
// Introduction:
// TypedColumn exists to allow efficient access to the data in a Column without
// having to go through dynamic type checking. There are two main reasons for
// this:
// 1. Performance: dynamic type checking is not free and so if this is used
//    in a particularily hot codepath, the typechecking can be a significant
//    overhead.
// 2. Ergonomics: having to convert back and forth from/to SqlValue causes
//    signifcant clutter in parts of the code which can already be quite hard
//    to follow (e.g. trackers like StackProfileTracker which perform cross
//    checking of various ids).
//
// Implementation:
// TypedColumn is implemented as a memberless subclass of Column. This allows
// us to reinterpret case from a Column* to a TypedColumn<T> where we know the
// type T. The methods of TypedColumn are type-specialized methods of Column
// which allow callers to pass raw types instead of using SqlValue.
//
// There are two helper classes (tc_internal::TypeHandler and
// tc_internal::Serializer) where we specialize behaviour which needs to be
// different based on T. See their class documentation and below for details
// on their purpose.
template <typename T>
struct TypedColumn : public Column {
 private:
  using TH = tc_internal::TypeHandler<T>;

  // The non-optional type of the data in this column.
  using non_optional_type =
      typename tc_internal::TypeHandler<T>::non_optional_type;

  // The type of data in this column (including Optional wrapper if the type
  // should be optional).
  using get_type = typename tc_internal::TypeHandler<T>::get_type;

  // The type which should be passed to SqlValue functions.
  using sql_value_type = typename tc_internal::TypeHandler<T>::sql_value_type;

  using Serializer = tc_internal::Serializer<non_optional_type>;

 public:
  // The type which should be stored in the SparseVector.
  // Used by the macro code when actually constructing the SparseVectors.
  using serialized_type = typename Serializer::serialized_type;

  get_type operator[](uint32_t row) const {
    return Serializer::Deserialize(
        TH::Get(sparse_vector(), row_map().Get(row)));
  }

  // Special function only for string types to allow retrieving the string
  // directly from the column.
  template <bool is_string = TH::is_string>
  typename std::enable_if<is_string, NullTermStringView>::type GetString(
      uint32_t row) const {
    return string_pool().Get(sparse_vector().GetNonNull(row_map().Get(row)));
  }

  // Sets the data in the column at index |row|.
  void Set(uint32_t row, non_optional_type v) {
    auto serialized = Serializer::Serialize(v);
    mutable_sparse_vector()->Set(row_map().Get(row), serialized);
  }

  // Inserts the value at the end of the column.
  void Append(T v) {
    mutable_sparse_vector()->Append(Serializer::Serialize(v));
  }

  // Returns the row containing the given value in the Column.
  base::Optional<uint32_t> IndexOf(sql_value_type v) const {
    return Column::IndexOf(ToValue(v));
  }

  std::vector<get_type> ToVectorForTesting() const {
    std::vector<T> result(row_map().size());
    for (uint32_t i = 0; i < row_map().size(); ++i)
      result[i] = (*this)[i];
    return result;
  }

  // Helper functions to create constraints for the given value.
  Constraint eq(sql_value_type v) const { return eq_value(ToValue(v)); }
  Constraint gt(sql_value_type v) const { return gt_value(ToValue(v)); }
  Constraint lt(sql_value_type v) const { return lt_value(ToValue(v)); }
  Constraint ne(sql_value_type v) const { return ne_value(ToValue(v)); }
  Constraint ge(sql_value_type v) const { return ge_value(ToValue(v)); }
  Constraint le(sql_value_type v) const { return le_value(ToValue(v)); }

  // Implements equality between two items of type |T|.
  static constexpr bool Equals(T a, T b) { return TH::Equals(a, b); }

  // Encodes the default flags for a column of the current type.
  static constexpr uint32_t default_flags() {
    return TH::is_optional ? Flag::kNoFlag : Flag::kNonNull;
  }

  // Converts the static type T into the dynamic SqlValue type of this column.
  static SqlValue::Type SqlValueType() {
    return Column::ToSqlValueType<serialized_type>();
  }

  // Reinterpret cast a Column to TypedColumn or crash if that is likely to be
  // unsafe.
  static const TypedColumn<T>* FromColumn(const Column* column) {
    if (column->IsColumnType<serialized_type>() &&
        (column->IsNullable() == TH::is_optional) && !column->IsId()) {
      return reinterpret_cast<const TypedColumn<T>*>(column);
    } else {
      PERFETTO_FATAL("Unsafe to convert Column to TypedColumn.");
    }
  }

 private:
  static SqlValue ToValue(double value) { return SqlValue::Double(value); }
  static SqlValue ToValue(uint32_t value) { return SqlValue::Long(value); }
  static SqlValue ToValue(int64_t value) { return SqlValue::Long(value); }
  static SqlValue ToValue(NullTermStringView value) {
    return SqlValue::String(value.c_str());
  }

  const SparseVector<serialized_type>& sparse_vector() const {
    return Column::sparse_vector<serialized_type>();
  }
  SparseVector<serialized_type>* mutable_sparse_vector() {
    return Column::mutable_sparse_vector<serialized_type>();
  }
};

// Represents a column containing ids.
// TODO(lalitm): think about unifying this with TypedColumn in the
// future.
template <typename Id>
struct IdColumn : public Column {
  Id operator[](uint32_t row) const { return Id(row_map().Get(row)); }
  base::Optional<uint32_t> IndexOf(Id id) const {
    return row_map().IndexOf(id.value);
  }

  // Helper functions to create constraints for the given value.
  Constraint eq(uint32_t v) const { return eq_value(SqlValue::Long(v)); }
  Constraint gt(uint32_t v) const { return gt_value(SqlValue::Long(v)); }
  Constraint lt(uint32_t v) const { return lt_value(SqlValue::Long(v)); }
  Constraint ne(uint32_t v) const { return ne_value(SqlValue::Long(v)); }
  Constraint ge(uint32_t v) const { return ge_value(SqlValue::Long(v)); }
  Constraint le(uint32_t v) const { return le_value(SqlValue::Long(v)); }
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_DB_TYPED_COLUMN_H_
