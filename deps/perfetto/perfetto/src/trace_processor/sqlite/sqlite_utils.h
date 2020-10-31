/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_SQLITE_SQLITE_UTILS_H_
#define SRC_TRACE_PROCESSOR_SQLITE_SQLITE_UTILS_H_

#include <math.h>
#include <sqlite3.h>

#include <functional>
#include <limits>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/optional.h"
#include "src/trace_processor/sqlite/scoped_db.h"
#include "src/trace_processor/sqlite/sqlite_table.h"

namespace perfetto {
namespace trace_processor {
namespace sqlite_utils {

const auto kSqliteStatic = reinterpret_cast<sqlite3_destructor_type>(0);
const auto kSqliteTransient = reinterpret_cast<sqlite3_destructor_type>(-1);

template <typename T>
using is_numeric =
    typename std::enable_if<std::is_arithmetic<T>::value, T>::type;

template <typename T>
using is_float =
    typename std::enable_if<std::is_floating_point<T>::value, T>::type;

template <typename T>
using is_int = typename std::enable_if<std::is_integral<T>::value, T>::type;

inline bool IsOpEq(int op) {
  return op == SQLITE_INDEX_CONSTRAINT_EQ;
}

inline bool IsOpGe(int op) {
  return op == SQLITE_INDEX_CONSTRAINT_GE;
}

inline bool IsOpGt(int op) {
  return op == SQLITE_INDEX_CONSTRAINT_GT;
}

inline bool IsOpLe(int op) {
  return op == SQLITE_INDEX_CONSTRAINT_LE;
}

inline bool IsOpLt(int op) {
  return op == SQLITE_INDEX_CONSTRAINT_LT;
}

inline std::string OpToString(int op) {
  switch (op) {
    case SQLITE_INDEX_CONSTRAINT_EQ:
      return "=";
    case SQLITE_INDEX_CONSTRAINT_NE:
      return "!=";
    case SQLITE_INDEX_CONSTRAINT_GE:
      return ">=";
    case SQLITE_INDEX_CONSTRAINT_GT:
      return ">";
    case SQLITE_INDEX_CONSTRAINT_LE:
      return "<=";
    case SQLITE_INDEX_CONSTRAINT_LT:
      return "<";
    case SQLITE_INDEX_CONSTRAINT_LIKE:
      return "like";
    default:
      PERFETTO_FATAL("Operator to string conversion not impemented for %d", op);
  }
}

inline bool IsOpIsNull(int op) {
  return op == SQLITE_INDEX_CONSTRAINT_ISNULL;
}

inline bool IsOpIsNotNull(int op) {
  return op == SQLITE_INDEX_CONSTRAINT_ISNOTNULL;
}

template <typename T>
T ExtractSqliteValue(sqlite3_value* value);

template <>
inline uint8_t ExtractSqliteValue(sqlite3_value* value) {
  auto type = sqlite3_value_type(value);
  PERFETTO_DCHECK(type == SQLITE_INTEGER);
  return static_cast<uint8_t>(sqlite3_value_int(value));
}

template <>
inline uint32_t ExtractSqliteValue(sqlite3_value* value) {
  auto type = sqlite3_value_type(value);
  PERFETTO_DCHECK(type == SQLITE_INTEGER);
  return static_cast<uint32_t>(sqlite3_value_int64(value));
}

template <>
inline int32_t ExtractSqliteValue(sqlite3_value* value) {
  auto type = sqlite3_value_type(value);
  PERFETTO_DCHECK(type == SQLITE_INTEGER);
  return sqlite3_value_int(value);
}

template <>
inline int64_t ExtractSqliteValue(sqlite3_value* value) {
  auto type = sqlite3_value_type(value);
  PERFETTO_DCHECK(type == SQLITE_INTEGER);
  return static_cast<int64_t>(sqlite3_value_int64(value));
}

template <>
inline double ExtractSqliteValue(sqlite3_value* value) {
  auto type = sqlite3_value_type(value);
  PERFETTO_DCHECK(type == SQLITE_FLOAT || type == SQLITE_INTEGER);
  return sqlite3_value_double(value);
}

template <>
inline bool ExtractSqliteValue(sqlite3_value* value) {
  auto type = sqlite3_value_type(value);
  PERFETTO_DCHECK(type == SQLITE_INTEGER);
  return static_cast<bool>(sqlite3_value_int(value));
}

// Do not add a uint64_t version of ExtractSqliteValue. You should not be using
// uint64_t at all given that SQLite doesn't support it.

template <>
inline const char* ExtractSqliteValue(sqlite3_value* value) {
  auto type = sqlite3_value_type(value);
  PERFETTO_DCHECK(type == SQLITE_TEXT);
  return reinterpret_cast<const char*>(sqlite3_value_text(value));
}

template <>
inline std::string ExtractSqliteValue(sqlite3_value* value) {
  return ExtractSqliteValue<const char*>(value);
}

template <typename T>
class NumericPredicate {
 public:
  NumericPredicate(int op, T constant) : op_(op), constant_(constant) {}

  PERFETTO_ALWAYS_INLINE bool operator()(T other) const {
    switch (op_) {
      case SQLITE_INDEX_CONSTRAINT_ISNULL:
        return false;
      case SQLITE_INDEX_CONSTRAINT_ISNOTNULL:
        return true;
      case SQLITE_INDEX_CONSTRAINT_EQ:
      case SQLITE_INDEX_CONSTRAINT_IS:
        return std::equal_to<T>()(other, constant_);
      case SQLITE_INDEX_CONSTRAINT_NE:
      case SQLITE_INDEX_CONSTRAINT_ISNOT:
        return std::not_equal_to<T>()(other, constant_);
      case SQLITE_INDEX_CONSTRAINT_GE:
        return std::greater_equal<T>()(other, constant_);
      case SQLITE_INDEX_CONSTRAINT_GT:
        return std::greater<T>()(other, constant_);
      case SQLITE_INDEX_CONSTRAINT_LE:
        return std::less_equal<T>()(other, constant_);
      case SQLITE_INDEX_CONSTRAINT_LT:
        return std::less<T>()(other, constant_);
      default:
        PERFETTO_FATAL("For GCC");
    }
  }

 private:
  int op_;
  T constant_;
};

template <typename T, typename sqlite_utils::is_numeric<T>* = nullptr>
NumericPredicate<T> CreateNumericPredicate(int op, sqlite3_value* value) {
  T extracted =
      IsOpIsNull(op) || IsOpIsNotNull(op) ? 0 : ExtractSqliteValue<T>(value);
  return NumericPredicate<T>(op, extracted);
}

inline std::function<bool(const char*)> CreateStringPredicate(
    int op,
    sqlite3_value* value) {
  switch (op) {
    case SQLITE_INDEX_CONSTRAINT_ISNULL:
      return [](const char* f) { return f == nullptr; };
    case SQLITE_INDEX_CONSTRAINT_ISNOTNULL:
      return [](const char* f) { return f != nullptr; };
  }

  const char* val = reinterpret_cast<const char*>(sqlite3_value_text(value));

  // If the value compared against is null, then to stay consistent with SQL
  // handling, we have to return false for non-null operators.
  if (val == nullptr) {
    PERFETTO_CHECK(op != SQLITE_INDEX_CONSTRAINT_IS &&
                   op != SQLITE_INDEX_CONSTRAINT_ISNOT);
    return [](const char*) { return false; };
  }

  switch (op) {
    case SQLITE_INDEX_CONSTRAINT_EQ:
    case SQLITE_INDEX_CONSTRAINT_IS:
      return [val](const char* str) {
        return str != nullptr && strcmp(str, val) == 0;
      };
    case SQLITE_INDEX_CONSTRAINT_NE:
    case SQLITE_INDEX_CONSTRAINT_ISNOT:
      return [val](const char* str) {
        return str != nullptr && strcmp(str, val) != 0;
      };
    case SQLITE_INDEX_CONSTRAINT_GE:
      return [val](const char* str) {
        return str != nullptr && strcmp(str, val) >= 0;
      };
    case SQLITE_INDEX_CONSTRAINT_GT:
      return [val](const char* str) {
        return str != nullptr && strcmp(str, val) > 0;
      };
    case SQLITE_INDEX_CONSTRAINT_LE:
      return [val](const char* str) {
        return str != nullptr && strcmp(str, val) <= 0;
      };
    case SQLITE_INDEX_CONSTRAINT_LT:
      return [val](const char* str) {
        return str != nullptr && strcmp(str, val) < 0;
      };
    case SQLITE_INDEX_CONSTRAINT_LIKE:
      return [val](const char* str) {
        return str != nullptr && sqlite3_strlike(val, str, 0) == 0;
      };
    case SQLITE_INDEX_CONSTRAINT_GLOB:
      return [val](const char* str) {
        return str != nullptr && sqlite3_strglob(val, str) == 0;
      };
    default:
      PERFETTO_FATAL("For GCC");
  }
}

// Greater bound for floating point numbers.
template <typename T, typename sqlite_utils::is_float<T>* = nullptr>
T FindGtBound(bool is_eq, sqlite3_value* sqlite_val) {
  constexpr auto kMax = static_cast<long double>(std::numeric_limits<T>::max());
  auto type = sqlite3_value_type(sqlite_val);
  if (type != SQLITE_INTEGER && type != SQLITE_FLOAT) {
    return kMax;
  }

  // If this is a strict gt bound then just get the next highest float
  // after value.
  auto value = ExtractSqliteValue<T>(sqlite_val);
  return is_eq ? value : nexttoward(value, kMax);
}

template <typename T, typename sqlite_utils::is_int<T>* = nullptr>
T FindGtBound(bool is_eq, sqlite3_value* sqlite_val) {
  auto type = sqlite3_value_type(sqlite_val);
  if (type == SQLITE_INTEGER) {
    auto value = ExtractSqliteValue<T>(sqlite_val);
    return is_eq ? value : value + 1;
  } else if (type == SQLITE_FLOAT) {
    auto value = ExtractSqliteValue<double>(sqlite_val);
    auto above = ceil(value);
    auto cast = static_cast<T>(above);
    return value < above ? cast : (is_eq ? cast : cast + 1);
  } else {
    return std::numeric_limits<T>::max();
  }
}

template <typename T, typename sqlite_utils::is_float<T>* = nullptr>
T FindLtBound(bool is_eq, sqlite3_value* sqlite_val) {
  constexpr auto kMin =
      static_cast<long double>(std::numeric_limits<T>::lowest());
  auto type = sqlite3_value_type(sqlite_val);
  if (type != SQLITE_INTEGER && type != SQLITE_FLOAT) {
    return kMin;
  }

  // If this is a strict lt bound then just get the next lowest float
  // before value.
  auto value = ExtractSqliteValue<T>(sqlite_val);
  return is_eq ? value : nexttoward(value, kMin);
}

template <typename T, typename sqlite_utils::is_int<T>* = nullptr>
T FindLtBound(bool is_eq, sqlite3_value* sqlite_val) {
  auto type = sqlite3_value_type(sqlite_val);
  if (type == SQLITE_INTEGER) {
    auto value = ExtractSqliteValue<T>(sqlite_val);
    return is_eq ? value : value - 1;
  } else if (type == SQLITE_FLOAT) {
    auto value = ExtractSqliteValue<double>(sqlite_val);
    auto below = floor(value);
    auto cast = static_cast<T>(below);
    return value > below ? cast : (is_eq ? cast : cast - 1);
  } else {
    return std::numeric_limits<T>::max();
  }
}

template <typename T, typename sqlite_utils::is_float<T>* = nullptr>
T FindEqBound(sqlite3_value* sqlite_val) {
  auto type = sqlite3_value_type(sqlite_val);
  if (type != SQLITE_INTEGER && type != SQLITE_FLOAT) {
    return std::numeric_limits<T>::max();
  }
  return ExtractSqliteValue<T>(sqlite_val);
}

template <typename T, typename sqlite_utils::is_int<T>* = nullptr>
T FindEqBound(sqlite3_value* sqlite_val) {
  auto type = sqlite3_value_type(sqlite_val);
  if (type == SQLITE_INTEGER) {
    return ExtractSqliteValue<T>(sqlite_val);
  } else if (type == SQLITE_FLOAT) {
    auto value = ExtractSqliteValue<double>(sqlite_val);
    auto below = floor(value);
    auto cast = static_cast<T>(below);
    return value > below ? std::numeric_limits<T>::max() : cast;
  } else {
    return std::numeric_limits<T>::max();
  }
}

template <typename T>
void ReportSqliteResult(sqlite3_context*, T value);

// Do not add a uint64_t version of ReportSqliteResult. You should not be using
// uint64_t at all given that SQLite doesn't support it.

template <>
inline void ReportSqliteResult(sqlite3_context* ctx, int32_t value) {
  sqlite3_result_int(ctx, value);
}

template <>
inline void ReportSqliteResult(sqlite3_context* ctx, int64_t value) {
  sqlite3_result_int64(ctx, value);
}

template <>
inline void ReportSqliteResult(sqlite3_context* ctx, uint8_t value) {
  sqlite3_result_int(ctx, value);
}

template <>
inline void ReportSqliteResult(sqlite3_context* ctx, uint32_t value) {
  sqlite3_result_int64(ctx, value);
}

template <>
inline void ReportSqliteResult(sqlite3_context* ctx, bool value) {
  sqlite3_result_int(ctx, value);
}

template <>
inline void ReportSqliteResult(sqlite3_context* ctx, double value) {
  sqlite3_result_double(ctx, value);
}

inline std::string SqliteValueAsString(sqlite3_value* value) {
  switch (sqlite3_value_type(value)) {
    case SQLITE_INTEGER:
      return std::to_string(sqlite3_value_int64(value));
    case SQLITE_FLOAT:
      return std::to_string(sqlite3_value_double(value));
    case SQLITE_TEXT: {
      const char* str =
          reinterpret_cast<const char*>(sqlite3_value_text(value));
      return "'" + std::string(str) + "'";
    }
    default:
      PERFETTO_FATAL("Unknown value type %d", sqlite3_value_type(value));
  }
}

inline std::vector<SqliteTable::Column> GetColumnsForTable(
    sqlite3* db,
    const std::string& raw_table_name) {
  char sql[1024];
  const char kRawSql[] = "SELECT name, type from pragma_table_info(\"%s\")";

  // Support names which are table valued functions with arguments.
  std::string table_name = raw_table_name.substr(0, raw_table_name.find('('));
  int n = snprintf(sql, sizeof(sql), kRawSql, table_name.c_str());
  PERFETTO_DCHECK(n >= 0 || static_cast<size_t>(n) < sizeof(sql));

  sqlite3_stmt* raw_stmt = nullptr;
  int err = sqlite3_prepare_v2(db, sql, n, &raw_stmt, nullptr);
  if (err != SQLITE_OK) {
    PERFETTO_ELOG("Preparing database failed");
    return {};
  }
  ScopedStmt stmt(raw_stmt);
  PERFETTO_DCHECK(sqlite3_column_count(*stmt) == 2);

  std::vector<SqliteTable::Column> columns;
  for (;;) {
    err = sqlite3_step(raw_stmt);
    if (err == SQLITE_DONE)
      break;
    if (err != SQLITE_ROW) {
      PERFETTO_ELOG("Querying schema of table %s failed",
                    raw_table_name.c_str());
      return {};
    }

    const char* name =
        reinterpret_cast<const char*>(sqlite3_column_text(*stmt, 0));
    const char* raw_type =
        reinterpret_cast<const char*>(sqlite3_column_text(*stmt, 1));
    if (!name || !raw_type || !*name) {
      PERFETTO_FATAL("Schema for %s has invalid column values",
                     raw_table_name.c_str());
    }

    SqlValue::Type type;
    if (strcmp(raw_type, "STRING") == 0) {
      type = SqlValue::Type::kString;
    } else if (strcmp(raw_type, "DOUBLE") == 0) {
      type = SqlValue::Type::kDouble;
    } else if (strcmp(raw_type, "BIG INT") == 0 ||
               strcmp(raw_type, "UNSIGNED INT") == 0 ||
               strcmp(raw_type, "INT") == 0 ||
               strcmp(raw_type, "BOOLEAN") == 0) {
      type = SqlValue::Type::kLong;
    } else if (!*raw_type) {
      PERFETTO_DLOG("Unknown column type for %s %s", raw_table_name.c_str(),
                    name);
      type = SqlValue::Type::kNull;
    } else {
      PERFETTO_FATAL("Unknown column type '%s' on table %s", raw_type,
                     raw_table_name.c_str());
    }
    columns.emplace_back(columns.size(), name, type);
  }
  return columns;
}

template <typename T>
int CompareValuesAsc(const T& f, const T& s) {
  return f < s ? -1 : (f > s ? 1 : 0);
}

template <typename T>
int CompareValuesDesc(const T& f, const T& s) {
  return -CompareValuesAsc(f, s);
}

}  // namespace sqlite_utils
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SQLITE_SQLITE_UTILS_H_
