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

#ifndef SRC_TRACE_PROCESSOR_DB_TABLE_H_
#define SRC_TRACE_PROCESSOR_DB_TABLE_H_

#include <stdint.h>

#include <limits>
#include <numeric>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/optional.h"
#include "src/trace_processor/containers/string_pool.h"
#include "src/trace_processor/db/column.h"
#include "src/trace_processor/db/typed_column.h"

namespace perfetto {
namespace trace_processor {

// Represents a table of data with named, strongly typed columns.
class Table {
 public:
  // Iterator over the rows of the table.
  class Iterator {
   public:
    Iterator(const Table* table) : table_(table) {
      for (const auto& rm : table->row_maps()) {
        its_.emplace_back(rm.IterateRows());
      }
    }

    Iterator(Iterator&&) noexcept = default;
    Iterator& operator=(Iterator&&) = default;

    // Advances the iterator to the next row of the table.
    void Next() {
      for (auto& it : its_) {
        it.Next();
      }
    }

    // Returns whether the row the iterator is pointing at is valid.
    operator bool() const { return its_[0]; }

    // Returns the value at the current row for column |col_idx|.
    SqlValue Get(uint32_t col_idx) const {
      const auto& col = table_->columns_[col_idx];
      return col.GetAtIdx(its_[col.row_map_idx_].row());
    }

   private:
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    const Table* table_ = nullptr;
    std::vector<RowMap::Iterator> its_;
  };

  // Helper class storing the schema of the table. This allows decisions to be
  // made about operations on the table without materializing the table - this
  // may be expensive for dynamically computed tables.
  //
  // Subclasses of Table usually provide a method (named Schema()) to statically
  // generate an instance of this class.
  struct Schema {
    struct Column {
      std::string name;
      SqlValue::Type type;
      bool is_id;
      bool is_sorted;
      bool is_hidden;
    };
    std::vector<Column> columns;
  };

  Table();
  virtual ~Table();

  // We explicitly define the move constructor here because we need to update
  // the Table pointer in each column in the table.
  Table(Table&& other) noexcept { *this = std::move(other); }
  Table& operator=(Table&& other) noexcept;

  // Filters the Table using the specified filter constraints.
  Table Filter(
      const std::vector<Constraint>& cs,
      RowMap::OptimizeFor optimize_for = RowMap::OptimizeFor::kMemory) const {
    return Apply(FilterToRowMap(cs, optimize_for));
  }

  // Filters the Table using the specified filter constraints optionally
  // specifying what the returned RowMap should optimize for.
  // Returns a RowMap which, if applied to the table, would contain the rows
  // post filter.
  RowMap FilterToRowMap(
      const std::vector<Constraint>& cs,
      RowMap::OptimizeFor optimize_for = RowMap::OptimizeFor::kMemory) const {
    RowMap rm(0, row_count_, optimize_for);
    for (const Constraint& c : cs) {
      columns_[c.col_idx].FilterInto(c.op, c.value, &rm);
    }
    return rm;
  }

  // Applies the given RowMap to the current table by picking out the rows
  // specified in the RowMap to be present in the output table.
  // Note: the RowMap should not reorder this table; this is guaranteed if the
  // passed RowMap is generated using |FilterToRowMap|.
  Table Apply(RowMap rm) const {
    Table table = CopyExceptRowMaps();
    table.row_count_ = rm.size();
    for (const RowMap& map : row_maps_) {
      table.row_maps_.emplace_back(map.SelectRows(rm));
      PERFETTO_DCHECK(table.row_maps_.back().size() == table.row_count());
    }
    return table;
  }

  // Sorts the Table using the specified order by constraints.
  Table Sort(const std::vector<Order>& od) const;

  // Joins |this| table with the |other| table using the values of column |left|
  // of |this| table to lookup the row in |right| column of the |other| table.
  //
  // Concretely, for each row in the returned table we lookup the value of
  // |left| in |right|. The found row is used as the values for |other|'s
  // columns in the returned table.
  //
  // This means we obtain the following invariants:
  //  1. this->size() == ret->size()
  //  2. this->Rows()[i].Get(j) == ret->Rows()[i].Get(j)
  //
  // It also means there are few restrictions on the data in |left| and |right|:
  //  * |left| is not allowed to have any nulls.
  //  * |left|'s values must exist in |right|
  Table LookupJoin(JoinKey left, const Table& other, JoinKey right);

  template <typename T>
  Table ExtendWithColumn(const char* name,
                         std::unique_ptr<SparseVector<T>> sv,
                         uint32_t flags) const {
    PERFETTO_DCHECK(sv->size() == row_count_);
    uint32_t size = sv->size();
    uint32_t row_map_count = static_cast<uint32_t>(row_maps_.size());
    Table ret = Copy();
    ret.columns_.push_back(Column::WithOwnedStorage(
        name, std::move(sv), flags, &ret, GetColumnCount(), row_map_count));
    ret.row_maps_.emplace_back(RowMap(0, size));
    return ret;
  }

  // Returns the column at index |idx| in the Table.
  const Column& GetColumn(uint32_t idx) const { return columns_[idx]; }

  // Returns the column with the given name or nullptr otherwise.
  const Column* GetColumnByName(const char* name) const {
    auto it = std::find_if(
        columns_.begin(), columns_.end(),
        [name](const Column& col) { return strcmp(col.name(), name) == 0; });
    if (it == columns_.end())
      return nullptr;
    return &*it;
  }

  template <typename T>
  const TypedColumn<T>* GetTypedColumnByName(const char* name) const {
    return TypedColumn<T>::FromColumn(GetColumnByName(name));
  }

  // Returns the number of columns in the Table.
  uint32_t GetColumnCount() const {
    return static_cast<uint32_t>(columns_.size());
  }

  // Returns an iterator into the Table.
  Iterator IterateRows() const { return Iterator(this); }

  uint32_t row_count() const { return row_count_; }
  const std::vector<RowMap>& row_maps() const { return row_maps_; }

 protected:
  Table(StringPool* pool, const Table* parent);

  std::vector<RowMap> row_maps_;
  std::vector<Column> columns_;
  uint32_t row_count_ = 0;

  StringPool* string_pool_ = nullptr;

 private:
  friend class Column;

  Table Copy() const;
  Table CopyExceptRowMaps() const;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_DB_TABLE_H_
