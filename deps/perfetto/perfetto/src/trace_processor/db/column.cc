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

#include "src/trace_processor/db/column.h"

#include "src/trace_processor/db/compare.h"
#include "src/trace_processor/db/table.h"

namespace perfetto {
namespace trace_processor {

Column::Column(const Column& column,
               Table* table,
               uint32_t col_idx,
               uint32_t row_map_idx)
    : Column(column.name_,
             column.type_,
             column.flags_,
             table,
             col_idx,
             row_map_idx,
             column.sparse_vector_,
             column.owned_sparse_vector_) {}

Column::Column(const char* name,
               ColumnType type,
               uint32_t flags,
               Table* table,
               uint32_t col_idx_in_table,
               uint32_t row_map_idx,
               SparseVectorBase* sparse_vector,
               std::shared_ptr<SparseVectorBase> owned_sparse_vector)
    : owned_sparse_vector_(owned_sparse_vector),
      type_(type),
      sparse_vector_(sparse_vector),
      name_(name),
      flags_(flags),
      table_(table),
      col_idx_in_table_(col_idx_in_table),
      row_map_idx_(row_map_idx),
      string_pool_(table->string_pool_) {}

Column Column::IdColumn(Table* table, uint32_t col_idx, uint32_t row_map_idx) {
  return Column("id", ColumnType::kId, kIdFlags, table, col_idx, row_map_idx,
                nullptr, nullptr);
}

void Column::StableSort(bool desc, std::vector<uint32_t>* idx) const {
  if (desc) {
    StableSort<true /* desc */>(idx);
  } else {
    StableSort<false /* desc */>(idx);
  }
}

void Column::FilterIntoSlow(FilterOp op, SqlValue value, RowMap* rm) const {
  switch (type_) {
    case ColumnType::kInt32: {
      if (IsNullable()) {
        FilterIntoNumericSlow<int32_t, true /* is_nullable */>(op, value, rm);
      } else {
        FilterIntoNumericSlow<int32_t, false /* is_nullable */>(op, value, rm);
      }
      break;
    }
    case ColumnType::kUint32: {
      if (IsNullable()) {
        FilterIntoNumericSlow<uint32_t, true /* is_nullable */>(op, value, rm);
      } else {
        FilterIntoNumericSlow<uint32_t, false /* is_nullable */>(op, value, rm);
      }
      break;
    }
    case ColumnType::kInt64: {
      if (IsNullable()) {
        FilterIntoNumericSlow<int64_t, true /* is_nullable */>(op, value, rm);
      } else {
        FilterIntoNumericSlow<int64_t, false /* is_nullable */>(op, value, rm);
      }
      break;
    }
    case ColumnType::kDouble: {
      if (IsNullable()) {
        FilterIntoNumericSlow<double, true /* is_nullable */>(op, value, rm);
      } else {
        FilterIntoNumericSlow<double, false /* is_nullable */>(op, value, rm);
      }
      break;
    }
    case ColumnType::kString: {
      FilterIntoStringSlow(op, value, rm);
      break;
    }
    case ColumnType::kId: {
      FilterIntoIdSlow(op, value, rm);
      break;
    }
  }
}

template <typename T, bool is_nullable>
void Column::FilterIntoNumericSlow(FilterOp op,
                                   SqlValue value,
                                   RowMap* rm) const {
  PERFETTO_DCHECK(IsNullable() == is_nullable);
  PERFETTO_DCHECK(type_ == ToColumnType<T>());
  PERFETTO_DCHECK(std::is_arithmetic<T>::value);

  if (op == FilterOp::kIsNull) {
    PERFETTO_DCHECK(value.is_null());
    if (is_nullable) {
      row_map().FilterInto(rm, [this](uint32_t row) {
        return !sparse_vector<T>().Get(row).has_value();
      });
    } else {
      rm->Intersect(RowMap());
    }
    return;
  } else if (op == FilterOp::kIsNotNull) {
    PERFETTO_DCHECK(value.is_null());
    if (is_nullable) {
      row_map().FilterInto(rm, [this](uint32_t row) {
        return sparse_vector<T>().Get(row).has_value();
      });
    }
    return;
  }

  if (value.type == SqlValue::Type::kDouble) {
    double double_value = value.double_value;
    if (std::is_same<T, double>::value) {
      auto fn = [double_value](T v) {
        // We static cast here as this code will be compiled even when T ==
        // int64_t as we don't have if constexpr in C++11. In reality the cast
        // is a noop but we cannot statically verify that for the compiler.
        return compare::Numeric(static_cast<double>(v), double_value);
      };
      FilterIntoNumericWithComparatorSlow<T, is_nullable>(op, rm, fn);
    } else {
      auto fn = [double_value](T v) {
        // We static cast here as this code will be compiled even when T ==
        // double as we don't have if constexpr in C++11. In reality the cast is
        // a noop but we cannot statically verify that for the compiler.
        return compare::LongToDouble(static_cast<int64_t>(v), double_value);
      };
      FilterIntoNumericWithComparatorSlow<T, is_nullable>(op, rm, fn);
    }
  } else if (value.type == SqlValue::Type::kLong) {
    int64_t long_value = value.long_value;
    if (std::is_same<T, double>::value) {
      auto fn = [long_value](T v) {
        // We negate the return value as the long is always the first parameter
        // for this function even though the LHS of the comparator should
        // actually be |v|. This saves us having a duplicate implementation of
        // the comparision function.
        return -compare::LongToDouble(long_value, v);
      };
      FilterIntoNumericWithComparatorSlow<T, is_nullable>(op, rm, fn);
    } else {
      auto fn = [long_value](T v) {
        // We static cast here as this code will be compiled even when T ==
        // double as we don't have if constexpr in C++11. In reality the cast is
        // a noop but we cannot statically verify that for the compiler.
        return compare::Numeric(static_cast<int64_t>(v), long_value);
      };
      FilterIntoNumericWithComparatorSlow<T, is_nullable>(op, rm, fn);
    }
  } else {
    rm->Intersect(RowMap());
  }
}

template <typename T, bool is_nullable, typename Comparator>
void Column::FilterIntoNumericWithComparatorSlow(FilterOp op,
                                                 RowMap* rm,
                                                 Comparator cmp) const {
  switch (op) {
    case FilterOp::kLt:
      row_map().FilterInto(rm, [this, &cmp](uint32_t idx) {
        if (is_nullable) {
          auto opt_value = sparse_vector<T>().Get(idx);
          return opt_value && cmp(*opt_value) < 0;
        }
        return cmp(sparse_vector<T>().GetNonNull(idx)) < 0;
      });
      break;
    case FilterOp::kEq:
      row_map().FilterInto(rm, [this, &cmp](uint32_t idx) {
        if (is_nullable) {
          auto opt_value = sparse_vector<T>().Get(idx);
          return opt_value && cmp(*opt_value) == 0;
        }
        return cmp(sparse_vector<T>().GetNonNull(idx)) == 0;
      });
      break;
    case FilterOp::kGt:
      row_map().FilterInto(rm, [this, &cmp](uint32_t idx) {
        if (is_nullable) {
          auto opt_value = sparse_vector<T>().Get(idx);
          return opt_value && cmp(*opt_value) > 0;
        }
        return cmp(sparse_vector<T>().GetNonNull(idx)) > 0;
      });
      break;
    case FilterOp::kNe:
      row_map().FilterInto(rm, [this, &cmp](uint32_t idx) {
        if (is_nullable) {
          auto opt_value = sparse_vector<T>().Get(idx);
          return opt_value && cmp(*opt_value) != 0;
        }
        return cmp(sparse_vector<T>().GetNonNull(idx)) != 0;
      });
      break;
    case FilterOp::kLe:
      row_map().FilterInto(rm, [this, &cmp](uint32_t idx) {
        if (is_nullable) {
          auto opt_value = sparse_vector<T>().Get(idx);
          return opt_value && cmp(*opt_value) <= 0;
        }
        return cmp(sparse_vector<T>().GetNonNull(idx)) <= 0;
      });
      break;
    case FilterOp::kGe:
      row_map().FilterInto(rm, [this, &cmp](uint32_t idx) {
        if (is_nullable) {
          auto opt_value = sparse_vector<T>().Get(idx);
          return opt_value && cmp(*opt_value) >= 0;
        }
        return cmp(sparse_vector<T>().GetNonNull(idx)) >= 0;
      });
      break;
    case FilterOp::kIsNull:
    case FilterOp::kIsNotNull:
      PERFETTO_FATAL("Should be handled above");
  }
}

void Column::FilterIntoStringSlow(FilterOp op,
                                  SqlValue value,
                                  RowMap* rm) const {
  PERFETTO_DCHECK(type_ == ColumnType::kString);

  if (op == FilterOp::kIsNull) {
    PERFETTO_DCHECK(value.is_null());
    row_map().FilterInto(rm, [this](uint32_t row) {
      return GetStringPoolStringAtIdx(row).data() == nullptr;
    });
    return;
  } else if (op == FilterOp::kIsNotNull) {
    PERFETTO_DCHECK(value.is_null());
    row_map().FilterInto(rm, [this](uint32_t row) {
      return GetStringPoolStringAtIdx(row).data() != nullptr;
    });
    return;
  }

  if (value.type != SqlValue::Type::kString) {
    rm->Intersect(RowMap());
    return;
  }

  NullTermStringView str_value = value.string_value;
  PERFETTO_DCHECK(str_value.data() != nullptr);

  switch (op) {
    case FilterOp::kLt:
      row_map().FilterInto(rm, [this, str_value](uint32_t idx) {
        auto v = GetStringPoolStringAtIdx(idx);
        return v.data() != nullptr && compare::String(v, str_value) < 0;
      });
      break;
    case FilterOp::kEq:
      row_map().FilterInto(rm, [this, str_value](uint32_t idx) {
        auto v = GetStringPoolStringAtIdx(idx);
        return v.data() != nullptr && compare::String(v, str_value) == 0;
      });
      break;
    case FilterOp::kGt:
      row_map().FilterInto(rm, [this, str_value](uint32_t idx) {
        auto v = GetStringPoolStringAtIdx(idx);
        return v.data() != nullptr && compare::String(v, str_value) > 0;
      });
      break;
    case FilterOp::kNe:
      row_map().FilterInto(rm, [this, str_value](uint32_t idx) {
        auto v = GetStringPoolStringAtIdx(idx);
        return v.data() != nullptr && compare::String(v, str_value) != 0;
      });
      break;
    case FilterOp::kLe:
      row_map().FilterInto(rm, [this, str_value](uint32_t idx) {
        auto v = GetStringPoolStringAtIdx(idx);
        return v.data() != nullptr && compare::String(v, str_value) <= 0;
      });
      break;
    case FilterOp::kGe:
      row_map().FilterInto(rm, [this, str_value](uint32_t idx) {
        auto v = GetStringPoolStringAtIdx(idx);
        return v.data() != nullptr && compare::String(v, str_value) >= 0;
      });
      break;
    case FilterOp::kIsNull:
    case FilterOp::kIsNotNull:
      PERFETTO_FATAL("Should be handled above");
  }
}

void Column::FilterIntoIdSlow(FilterOp op, SqlValue value, RowMap* rm) const {
  PERFETTO_DCHECK(type_ == ColumnType::kId);

  if (op == FilterOp::kIsNull) {
    PERFETTO_DCHECK(value.is_null());
    rm->Intersect(RowMap());
    return;
  } else if (op == FilterOp::kIsNotNull) {
    PERFETTO_DCHECK(value.is_null());
    return;
  }

  if (value.type != SqlValue::Type::kLong) {
    rm->Intersect(RowMap());
    return;
  }

  uint32_t id_value = static_cast<uint32_t>(value.long_value);
  switch (op) {
    case FilterOp::kLt:
      row_map().FilterInto(rm, [id_value](uint32_t idx) {
        return compare::Numeric(idx, id_value) < 0;
      });
      break;
    case FilterOp::kEq:
      row_map().FilterInto(rm, [id_value](uint32_t idx) {
        return compare::Numeric(idx, id_value) == 0;
      });
      break;
    case FilterOp::kGt:
      row_map().FilterInto(rm, [id_value](uint32_t idx) {
        return compare::Numeric(idx, id_value) > 0;
      });
      break;
    case FilterOp::kNe:
      row_map().FilterInto(rm, [id_value](uint32_t idx) {
        return compare::Numeric(idx, id_value) != 0;
      });
      break;
    case FilterOp::kLe:
      row_map().FilterInto(rm, [id_value](uint32_t idx) {
        return compare::Numeric(idx, id_value) <= 0;
      });
      break;
    case FilterOp::kGe:
      row_map().FilterInto(rm, [id_value](uint32_t idx) {
        return compare::Numeric(idx, id_value) >= 0;
      });
      break;
    case FilterOp::kIsNull:
    case FilterOp::kIsNotNull:
      PERFETTO_FATAL("Should be handled above");
  }
}

template <bool desc>
void Column::StableSort(std::vector<uint32_t>* out) const {
  switch (type_) {
    case ColumnType::kInt32: {
      if (IsNullable()) {
        StableSortNumeric<desc, int32_t, true /* is_nullable */>(out);
      } else {
        StableSortNumeric<desc, int32_t, false /* is_nullable */>(out);
      }
      break;
    }
    case ColumnType::kUint32: {
      if (IsNullable()) {
        StableSortNumeric<desc, uint32_t, true /* is_nullable */>(out);
      } else {
        StableSortNumeric<desc, uint32_t, false /* is_nullable */>(out);
      }
      break;
    }
    case ColumnType::kInt64: {
      if (IsNullable()) {
        StableSortNumeric<desc, int64_t, true /* is_nullable */>(out);
      } else {
        StableSortNumeric<desc, int64_t, false /* is_nullable */>(out);
      }
      break;
    }
    case ColumnType::kDouble: {
      if (IsNullable()) {
        StableSortNumeric<desc, double, true /* is_nullable */>(out);
      } else {
        StableSortNumeric<desc, double, false /* is_nullable */>(out);
      }
      break;
    }
    case ColumnType::kString: {
      row_map().StableSort(out, [this](uint32_t a_idx, uint32_t b_idx) {
        auto a_str = GetStringPoolStringAtIdx(a_idx);
        auto b_str = GetStringPoolStringAtIdx(b_idx);

        int res = compare::NullableString(a_str, b_str);
        return desc ? res > 0 : res < 0;
      });
      break;
    }
    case ColumnType::kId:
      row_map().StableSort(out, [](uint32_t a_idx, uint32_t b_idx) {
        int res = compare::Numeric(a_idx, b_idx);
        return desc ? res > 0 : res < 0;
      });
  }
}

template <bool desc, typename T, bool is_nullable>
void Column::StableSortNumeric(std::vector<uint32_t>* out) const {
  PERFETTO_DCHECK(IsNullable() == is_nullable);
  PERFETTO_DCHECK(ToColumnType<T>() == type_);

  const auto& sv = sparse_vector<T>();
  row_map().StableSort(out, [&sv](uint32_t a_idx, uint32_t b_idx) {
    if (is_nullable) {
      auto a_val = sv.Get(a_idx);
      auto b_val = sv.Get(b_idx);

      int res = compare::NullableNumeric(a_val, b_val);
      return desc ? res > 0 : res < 0;
    }
    auto a_val = sv.GetNonNull(a_idx);
    auto b_val = sv.GetNonNull(b_idx);

    return desc ? compare::Numeric(a_val, b_val) > 0
                : compare::Numeric(a_val, b_val) < 0;
  });
}

const RowMap& Column::row_map() const {
  return table_->row_maps_[row_map_idx_];
}

}  // namespace trace_processor
}  // namespace perfetto
