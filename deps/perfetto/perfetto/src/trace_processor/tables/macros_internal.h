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

#ifndef SRC_TRACE_PROCESSOR_TABLES_MACROS_INTERNAL_H_
#define SRC_TRACE_PROCESSOR_TABLES_MACROS_INTERNAL_H_

#include <type_traits>

#include "src/trace_processor/db/table.h"
#include "src/trace_processor/db/typed_column.h"

namespace perfetto {
namespace trace_processor {
namespace macros_internal {

// We define this class to allow the table macro below to compile without
// needing templates; in reality none of the methods will be called because the
// pointer to this class will always be null.
class RootParentTable : public Table {
 public:
  struct Row {
   public:
    Row(std::nullptr_t) {}

    const char* type() const { return type_; }

   protected:
    const char* type_ = nullptr;
  };
  // This class only exists to allow typechecking to work correctly in Insert
  // below. If we had C++17 and if constexpr, we could statically verify that
  // this was never created but for now, we still need to define it to satisfy
  // the typechecker.
  struct IdAndRow {
    uint32_t id;
    uint32_t row;
  };
  IdAndRow Insert(const Row&) { PERFETTO_FATAL("Should not be called"); }
};

// IdHelper is used to figure out the Id type for a table.
//
// We do this using templates with the following algorithm:
// 1. If the parent class is anything but RootParentTable, the Id of the
//    table is the same as the Id of the parent.
// 2. If the parent class is RootParentTable (i.e. the table is a root
//    table), then the Id is the one defined in the table itself.
// The net result of this is that all tables in the hierarchy get the
// same type of Id - the one defined in the root table of that hierarchy.
//
// Reasoning: We do this because using uint32_t is very overloaded and
// having a wrapper type for ids is very helpful to avoid confusion with
// row indices (especially because ids and row indices often appear in
// similar places in the codebase - that is at insertion in parsers and
// in trackers).
template <typename ParentClass, typename Class>
struct IdHelper {
  using Id = typename ParentClass::Id;
};
template <typename Class>
struct IdHelper<RootParentTable, Class> {
  using Id = typename Class::DefinedId;
};

// The parent class for all macro generated tables.
// This class is used to extract common code from the macro tables to reduce
// code size.
class MacroTable : public Table {
 public:
  MacroTable(const char* name, StringPool* pool, Table* parent)
      : Table(pool, parent), name_(name), parent_(parent) {
    row_maps_.emplace_back();
    if (!parent) {
      columns_.emplace_back(
          Column::IdColumn(this, static_cast<uint32_t>(columns_.size()),
                           static_cast<uint32_t>(row_maps_.size()) - 1));
      columns_.emplace_back(
          Column("type", &type_, Column::kNoFlag, this,
                 static_cast<uint32_t>(columns_.size()),
                 static_cast<uint32_t>(row_maps_.size()) - 1));
    }
  }
  ~MacroTable() override;

  const char* table_name() const { return name_; }

 protected:
  void UpdateRowMapsAfterParentInsert() {
    if (parent_ != nullptr) {
      // If there is a parent table, add the last inserted row in each of the
      // parent row maps to the corresponding row map in the child.
      for (uint32_t i = 0; i < parent_->row_maps().size(); ++i) {
        const RowMap& parent_rm = parent_->row_maps()[i];
        row_maps_[i].Insert(parent_rm.Get(parent_rm.size() - 1));
      }
    }
    // Also add the index of the new row to the identity row map and increment
    // the size.
    row_maps_.back().Insert(row_count_++);
  }

  // Stores the most specific "derived" type of this row in the table.
  //
  // For example, suppose a row is inserted into the gpu_slice table. This will
  // also cause a row to be inserted into the slice table. For users querying
  // the slice table, they will want to know the "real" type of this slice (i.e.
  // they will want to see that the type is gpu_slice). This sparse vector
  // stores precisely the real type.
  //
  // Only relevant for parentless tables. Will be empty and unreferenced by
  // tables with parents.
  SparseVector<StringPool::Id> type_;

 private:
  const char* name_ = nullptr;
  Table* parent_ = nullptr;
};

}  // namespace macros_internal

// Ignore GCC warning about a missing argument for a variadic macro parameter.
#pragma GCC system_header

// Basic helper macros.
#define PERFETTO_TP_NOOP(...)

// Gets the class name from a table definition.
#define PERFETTO_TP_EXTRACT_TABLE_CLASS(class_name, ...) class_name
#define PERFETTO_TP_TABLE_CLASS(DEF) \
  DEF(PERFETTO_TP_EXTRACT_TABLE_CLASS, PERFETTO_TP_NOOP, PERFETTO_TP_NOOP)

// Gets the table name from the table definition.
#define PERFETTO_TP_EXTRACT_TABLE_NAME(_, table_name) table_name
#define PERFETTO_TP_TABLE_NAME(DEF) \
  DEF(PERFETTO_TP_EXTRACT_TABLE_NAME, PERFETTO_TP_NOOP, PERFETTO_TP_NOOP)

// Gets the parent definition from a table definition.
#define PERFETTO_TP_EXTRACT_PARENT_DEF(PARENT_DEF, _) PARENT_DEF
#define PERFETTO_TP_PARENT_DEF(DEF) \
  DEF(PERFETTO_TP_NOOP, PERFETTO_TP_EXTRACT_PARENT_DEF, PERFETTO_TP_NOOP)

// Invokes FN on each column in the definition of the table. We define a
// recursive macro as we need to walk up the hierarchy until we hit the root.
// Currently, we hardcode 5 levels but this can be increased as necessary.
#define PERFETTO_TP_ALL_COLUMNS_0(DEF, arg) \
  static_assert(false, "Macro recursion depth exceeded");
#define PERFETTO_TP_ALL_COLUMNS_1(DEF, arg) \
  DEF(PERFETTO_TP_NOOP, PERFETTO_TP_ALL_COLUMNS_0, arg)
#define PERFETTO_TP_ALL_COLUMNS_2(DEF, arg) \
  DEF(PERFETTO_TP_NOOP, PERFETTO_TP_ALL_COLUMNS_1, arg)
#define PERFETTO_TP_ALL_COLUMNS_3(DEF, arg) \
  DEF(PERFETTO_TP_NOOP, PERFETTO_TP_ALL_COLUMNS_2, arg)
#define PERFETTO_TP_ALL_COLUMNS_4(DEF, arg) \
  DEF(PERFETTO_TP_NOOP, PERFETTO_TP_ALL_COLUMNS_3, arg)
#define PERFETTO_TP_ALL_COLUMNS(DEF, arg) \
  DEF(PERFETTO_TP_NOOP, PERFETTO_TP_ALL_COLUMNS_4, arg)

// Invokes FN on each column in the table definition.
#define PERFETTO_TP_TABLE_COLUMNS(DEF, FN) \
  DEF(PERFETTO_TP_NOOP, PERFETTO_TP_NOOP, FN)

// Invokes FN on each column in every ancestor of the table.
#define PERFETTO_TP_PARENT_COLUMNS(DEF, FN) \
  PERFETTO_TP_ALL_COLUMNS(PERFETTO_TP_PARENT_DEF(DEF), FN)

// Basic macros for extracting column info from a schema.
#define PERFETTO_TP_NAME_COMMA(type, name, ...) name,
#define PERFETTO_TP_TYPE_NAME_COMMA(type, name, ...) type name,

// Constructor parameters of Table::Row.
// We name this name_c to avoid a clash with the field names of
// Table::Row.
#define PERFETTO_TP_ROW_CONSTRUCTOR(type, name, ...) type name##_c = {},

// Constructor parameters for parent of Row.
#define PERFETTO_TP_PARENT_ROW_CONSTRUCTOR(type, name, ...) name##_c,

// Initializes the members of Table::Row.
#define PERFETTO_TP_ROW_INITIALIZER(type, name, ...) name = name##_c;

// Defines the variable in Table::Row.
#define PERFETTO_TP_ROW_DEFINITION(type, name, ...) type name = {};

// Used to generate an equality implementation on Table::Row.
#define PERFETTO_TP_ROW_EQUALS(type, name, ...) \
  TypedColumn<type>::Equals(other.name, name)&&

// Defines the parent row field in Insert.
#define PERFETTO_TP_PARENT_ROW_INSERT(type, name, ...) row.name,

// Defines the member variable in the Table.
#define PERFETTO_TP_TABLE_MEMBER(type, name, ...) \
  SparseVector<TypedColumn<type>::serialized_type> name##_;

#define PERFETTO_TP_COLUMN_FLAG_HAS_FLAG_COL(type, name, flags) \
  case ColumnIndex::name:                                       \
    return static_cast<uint32_t>(flags) | TypedColumn<type>::default_flags();

#define PERFETTO_TP_COLUMN_FLAG_NO_FLAG_COL(type, name) \
  case ColumnIndex::name:                               \
    return TypedColumn<type>::default_flags();

#define PERFETTO_TP_COLUMN_FLAG_CHOOSER(type, name, maybe_flags, fn, ...) fn

#define PERFETTO_TP_COLUMN_FLAG(...)                                    \
  PERFETTO_TP_COLUMN_FLAG_CHOOSER(__VA_ARGS__,                          \
                                  PERFETTO_TP_COLUMN_FLAG_HAS_FLAG_COL, \
                                  PERFETTO_TP_COLUMN_FLAG_NO_FLAG_COL)  \
  (__VA_ARGS__)

// Invokes the chosen column constructor by passing the given args.
#define PERFETTO_TP_TABLE_CONSTRUCTOR_COLUMN(type, name, ...)               \
  columns_.emplace_back(#name, &name##_, FlagsForColumn(ColumnIndex::name), \
                        this, columns_.size(), row_maps_.size() - 1);

// Inserts the value into the corresponding column.
#define PERFETTO_TP_COLUMN_APPEND(type, name, ...) \
  mutable_##name()->Append(std::move(row.name));

// Creates a schema entry for the corresponding column.
#define PERFETTO_TP_COLUMN_SCHEMA(type, name, ...)          \
  schema.columns.emplace_back(Table::Schema::Column{        \
      #name, TypedColumn<type>::SqlValueType(), false,      \
      static_cast<bool>(FlagsForColumn(ColumnIndex::name) & \
                        Column::Flag::kSorted),             \
      static_cast<bool>(FlagsForColumn(ColumnIndex::name) & \
                        Column::Flag::kHidden)});

// Defines the accessors for a column.
#define PERFETTO_TP_TABLE_COL_ACCESSOR(type, name, ...)       \
  const TypedColumn<type>& name() const {                     \
    return static_cast<const TypedColumn<type>&>(             \
        columns_[static_cast<uint32_t>(ColumnIndex::name)]);  \
  }                                                           \
                                                              \
  TypedColumn<type>* mutable_##name() {                       \
    return static_cast<TypedColumn<type>*>(                   \
        &columns_[static_cast<uint32_t>(ColumnIndex::name)]); \
  }

// Definition used as the parent of root tables.
#define PERFETTO_TP_ROOT_TABLE_PARENT_DEF(NAME, PARENT, C) \
  NAME(macros_internal::RootParentTable, "root")

// For more general documentation, see PERFETTO_TP_TABLE in macros.h.
#define PERFETTO_TP_TABLE_INTERNAL(table_name, class_name, parent_class_name, \
                                   DEF)                                       \
  class class_name : public macros_internal::MacroTable {                     \
   private:                                                                   \
    /*                                                                        \
     * Allows IdHelper to access DefinedId for root tables.                   \
     * Needs to be defined here to allow the public using declaration of Id   \
     * below to work correctly.                                               \
     */                                                                       \
    friend struct macros_internal::IdHelper<parent_class_name, class_name>;   \
                                                                              \
    /*                                                                        \
     * Defines a new id type for a heirarchy of tables.                       \
     * We define it here as we need this type to be visible for the public    \
     * using declaration of Id below.                                         \
     * Note: This type will only used if this table is a root table.          \
     */                                                                       \
    struct DefinedId : public BaseId {                                        \
      DefinedId() = default;                                                  \
      explicit constexpr DefinedId(uint32_t v) : BaseId(v) {}                 \
    };                                                                        \
                                                                              \
   public:                                                                    \
    /*                                                                        \
     * This defines the type of the id to be the type of the root             \
     * table of the hierarchy - see IdHelper for more details.                \
     */                                                                       \
    using Id = macros_internal::IdHelper<parent_class_name, class_name>::Id;  \
    struct Row : parent_class_name::Row {                                     \
      /*                                                                      \
       * Expands to Row(col_type1 col1_c, base::Optional<col_type2> col2_c,   \
       * ...)                                                                 \
       */                                                                     \
      Row(PERFETTO_TP_ALL_COLUMNS(DEF, PERFETTO_TP_ROW_CONSTRUCTOR)           \
              std::nullptr_t = nullptr)                                       \
          : parent_class_name::Row(PERFETTO_TP_PARENT_COLUMNS(                \
                DEF,                                                          \
                PERFETTO_TP_PARENT_ROW_CONSTRUCTOR) nullptr) {                \
        type_ = table_name;                                                   \
                                                                              \
        /*                                                                    \
         * Expands to                                                         \
         * col1 = col1_c;                                                     \
         * col2 = col2_c;                                                     \
         * ...                                                                \
         */                                                                   \
        PERFETTO_TP_TABLE_COLUMNS(DEF, PERFETTO_TP_ROW_INITIALIZER)           \
      }                                                                       \
                                                                              \
      bool operator==(const class_name::Row& other) const {                   \
        return PERFETTO_TP_ALL_COLUMNS(DEF, PERFETTO_TP_ROW_EQUALS) true;     \
      }                                                                       \
                                                                              \
      /*                                                                      \
       * Expands to                                                           \
       * col_type1 col1 = {};                                                 \
       * base::Optional<col_type2> col2 = {};                                 \
       * ...                                                                  \
       */                                                                     \
      PERFETTO_TP_TABLE_COLUMNS(DEF, PERFETTO_TP_ROW_DEFINITION)              \
    };                                                                        \
                                                                              \
    enum class ColumnIndex : uint32_t {                                       \
      id,                                                                     \
      type, /* Expands to col1, col2, ... */                                  \
      PERFETTO_TP_ALL_COLUMNS(DEF, PERFETTO_TP_NAME_COMMA) kNumCols           \
    };                                                                        \
                                                                              \
    /* Return value of Insert giving access to id and row number */           \
    struct IdAndRow {                                                         \
      Id id;                                                                  \
      uint32_t row;                                                           \
    };                                                                        \
                                                                              \
    class_name(StringPool* pool, parent_class_name* parent)                   \
        : macros_internal::MacroTable(table_name, pool, parent),              \
          parent_(parent) {                                                   \
      /*                                                                      \
       * Expands to                                                           \
       * columns_.emplace_back("col1", col1_, Column::kNoFlag, this,          \
       *                        columns_.size(), row_maps_.size() - 1);       \
       * columns_.emplace_back("col2", col2_, Column::kNoFlag, this,          \
       *                       columns_.size(), row_maps_.size() - 1);        \
       * ...                                                                  \
       */                                                                     \
      PERFETTO_TP_TABLE_COLUMNS(DEF, PERFETTO_TP_TABLE_CONSTRUCTOR_COLUMN);   \
    }                                                                         \
    ~class_name() override;                                                   \
                                                                              \
    IdAndRow Insert(const Row& row) {                                         \
      Id id;                                                                  \
      uint32_t row_number = row_count();                                      \
      if (parent_ == nullptr) {                                               \
        id = Id{row_number};                                                  \
        type_.Append(string_pool_->InternString(row.type()));                 \
      } else {                                                                \
        id = Id{parent_->Insert(row).id};                                     \
      }                                                                       \
      UpdateRowMapsAfterParentInsert();                                       \
                                                                              \
      /*                                                                      \
       * Expands to                                                           \
       * col1_.Append(row.col1);                                              \
       * col2_.Append(row.col2);                                              \
       * ...                                                                  \
       */                                                                     \
      PERFETTO_TP_TABLE_COLUMNS(DEF, PERFETTO_TP_COLUMN_APPEND);              \
      return {id, row_number};                                                \
    }                                                                         \
                                                                              \
    const IdColumn<Id>& id() const {                                          \
      return static_cast<const IdColumn<Id>&>(                                \
          columns_[static_cast<uint32_t>(ColumnIndex::id)]);                  \
    }                                                                         \
                                                                              \
    const TypedColumn<StringPool::Id>& type() const {                         \
      return static_cast<const TypedColumn<StringPool::Id>&>(                 \
          columns_[static_cast<uint32_t>(ColumnIndex::type)]);                \
    }                                                                         \
                                                                              \
    static Table::Schema Schema() {                                           \
      Table::Schema schema;                                                   \
      schema.columns.emplace_back(Table::Schema::Column{                      \
          "id", SqlValue::Type::kLong, true, true, false});                   \
      schema.columns.emplace_back(Table::Schema::Column{                      \
          "type", SqlValue::Type::kString, false, false, false});             \
      PERFETTO_TP_ALL_COLUMNS(DEF, PERFETTO_TP_COLUMN_SCHEMA);                \
      return schema;                                                          \
    }                                                                         \
                                                                              \
    /*                                                                        \
     * Expands to                                                             \
     * const TypedColumn<col1_type>& col1() { return col1_; }                 \
     * TypedColumn<col1_type>* mutable_col1() { return &col1_; }              \
     * const TypedColumn<col2_type>& col2() { return col2_; }                 \
     * TypedColumn<col2_type>* mutable_col2() { return &col2_; }              \
     * ...                                                                    \
     */                                                                       \
    PERFETTO_TP_ALL_COLUMNS(DEF, PERFETTO_TP_TABLE_COL_ACCESSOR)              \
                                                                              \
   private:                                                                   \
    static uint32_t FlagsForColumn(const ColumnIndex index) {                 \
      switch (index) {                                                        \
        case ColumnIndex::kNumCols:                                           \
          PERFETTO_FATAL("Invalid index");                                    \
        case ColumnIndex::id:                                                 \
          return Column::kIdFlags;                                            \
        case ColumnIndex::type:                                               \
          return Column::kNoFlag;                                             \
          /*                                                                  \
           * Expands to:                                                      \
           *  case ColumnIndex::col1:                                         \
           *    return TypedColumn<col_type1>::default_flags();               \
           *  ...                                                             \
           */                                                                 \
          PERFETTO_TP_ALL_COLUMNS(DEF, PERFETTO_TP_COLUMN_FLAG)               \
      }                                                                       \
      PERFETTO_FATAL("For GCC");                                              \
    }                                                                         \
                                                                              \
    parent_class_name* parent_;                                               \
                                                                              \
    /*                                                                        \
     * Expands to                                                             \
     * SparseVector<col1_type> col1_;                                         \
     * SparseVector<col2_type> col2_;                                         \
     * ...                                                                    \
     */                                                                       \
    PERFETTO_TP_TABLE_COLUMNS(DEF, PERFETTO_TP_TABLE_MEMBER)                  \
  }

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_MACROS_INTERNAL_H_
