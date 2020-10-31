/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_DB_COMPARE_H_
#define SRC_TRACE_PROCESSOR_DB_COMPARE_H_

#include <algorithm>
#include <stdint.h>

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/trace_processor/basic_types.h"

namespace perfetto {
namespace trace_processor {
namespace compare {

// This file contains the de-facto impleemntation of all comparisions used by
// trace processor in every setting. All of this is centralised in one file to
// ensure both internal consistency with SQLite and consistency with SQLite.

// Compare a non-null numeric to a double; returns:
//  * <0 if i < d,
//  * >0 if i > d.
//  *  0 otherwise
// This code matches the behaviour of sqlite3IntFloatCompare.
inline int LongToDouble(int64_t i, double d) {
  // First check if we are out of range for a int64_t. We use the constants
  // directly instead of using numeric_limits as the casts introduces rounding
  // in the doubles as a double cannot exactly represent int64::max().
  if (d >= 9223372036854775808.0)
    return -1;
  if (d < -9223372036854775808.0)
    return 1;

  // Then, try to compare in int64 space to try and keep as much precision as
  // possible.
  int64_t d_i = static_cast<int64_t>(d);
  if (i < d_i)
    return -1;
  if (i > d_i)
    return 1;

  // Finally, try and compare in double space, sacrificing precision if
  // necessary.
  double i_d = static_cast<double>(i);
  return (i_d < d) ? -1 : (i_d > d ? 1 : 0);
}

// Compares two non-null numeric values; returns:
//  * <0 if a < b,
//  * >0 if a > b.
//  *  0 otherwise
// This code matches the behaviour of the inline code in the comparision path of
// sqlite3VdbeExec (for ints) and the behaviour of sqlite3MemCompare (for
// doubles).
template <typename T>
inline int Numeric(T a, T b) {
  static_assert(std::is_arithmetic<T>::value,
                "Numeric comparision performed with non-numeric type");
  return a < b ? -1 : (a > b ? 1 : 0);
}

// Compares two non-null bytes values; returns:
//  * <0 if a < b,
//  * >0 if a > b.
//  *  0 otherwise
// This code matches the behaviour of sqlite3BlobCompare.
inline int Bytes(const void* a, size_t a_n, const void* b, size_t b_n) {
  int res = memcmp(a, b, std::min(a_n, b_n));
  return res != 0 ? res
                  : static_cast<int>(static_cast<int64_t>(a_n) -
                                     static_cast<int64_t>(b_n));
}

// Compares two non-null string values; returns:
//  * <0 if a < b,
//  * >0 if a > b.
//  *  0 otherwise
// This code matches the behaviour of sqlite3BlobCompare which is called when
// there is no collation sequence defined in sqlite3MemCompare.
inline int String(base::StringView a, base::StringView b) {
  PERFETTO_DCHECK(a.data() != nullptr);
  PERFETTO_DCHECK(b.data() != nullptr);
  return Bytes(a.data(), a.size(), b.data(), b.size());
}

// Compares two nullable numeric values; returns:
//  *  0 if both a and b are null
//  * <0 if a is null and b is non null
//  * >0 if a is non null and b is null
//  * <0 if a < b (a and b both non null)
//  * >0 if a > b (a and b both non null)
//  *  0 otherwise
// Should only be used for defining an ordering on value of type T. For filter
// functions, compare::Numeric should be used above after checking if the value
// is null.
// This method was defined from observing the behaviour of SQLite when sorting
// on columns containing nulls.
template <typename T>
inline int NullableNumeric(base::Optional<T> a, base::Optional<T> b) {
  if (!a)
    return b ? -1 : 0;

  if (!b)
    return 1;

  return compare::Numeric(*a, *b);
}

// Compares two strings, either of which can be null; returns:
//  *  0 if both a and b are null
//  * <0 if a is null and b is non null
//  * >0 if a is non null and b is null
//  * <0 if a < b (a and b both non null)
//  * >0 if a > b (a and b both non null)
//  *  0 otherwise
// Should only be used for defining an ordering on value of type T. For filter
// functions, compare::String should be used above after checking if the value
// is null.
// This method was defined from observing the behaviour of SQLite when sorting
// on columns containing nulls.
inline int NullableString(base::StringView a, base::StringView b) {
  if (!a.data())
    return b.data() ? -1 : 0;

  if (!b.data())
    return 1;

  return compare::String(a, b);
}

// Compares two SqlValue; returns:
//  * <0 if a.type < b.type and a and b are not both numeric
//  * >0 if a.type > b.type and a and b are not both numeric
//  * <0 if a < b (a and b both non null and either of same type or numeric)
//  * >0 if a > b (a and b both non null and either of same type or numeric)
//  *  0 otherwise
// This code roughly matches the behaviour of the code in the comparision path
// of sqlite3VdbeExec except we are intentionally more strict than SQLite when
// it comes to casting between strings and numerics - we disallow moving between
// the two. We do allow comparing between doubles and longs however as doubles
// can easily appear by using constants in SQL even when intending to use longs.
inline int SqlValue(const SqlValue& a, const SqlValue& b) {
  if (a.type != b.type) {
    if (a.type == SqlValue::kLong && b.type == SqlValue::kDouble)
      return compare::LongToDouble(a.long_value, b.double_value);

    if (a.type == SqlValue::kDouble && b.type == SqlValue::kLong)
      return -compare::LongToDouble(b.long_value, a.double_value);

    return a.type - b.type;
  }

  switch (a.type) {
    case SqlValue::Type::kLong:
      return compare::Numeric(a.long_value, b.long_value);
    case SqlValue::Type::kDouble:
      return compare::Numeric(a.double_value, b.double_value);
    case SqlValue::Type::kString:
      return compare::String(a.string_value, b.string_value);
    case SqlValue::Type::kBytes:
      return compare::Bytes(a.bytes_value, a.bytes_count, b.bytes_value,
                            b.bytes_count);
    case SqlValue::Type::kNull:
      return 0;
  }
  PERFETTO_FATAL("For GCC");
}

// Implements a comparator for SqlValues to use for std algorithms.
// See documentation of compare::SqlValue for details of how this function
// works.
inline int SqlValueComparator(const trace_processor::SqlValue& a,
                              const trace_processor::SqlValue& b) {
  return compare::SqlValue(a, b) < 0;
}

}  // namespace compare
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_DB_COMPARE_H_
