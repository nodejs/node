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

#include "src/trace_processor/dynamic/describe_slice_generator.h"

#include "src/trace_processor/analysis/describe_slice.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

namespace {

DescribeSliceGenerator::InputValues GetDescribeSliceInputValues(
    const std::vector<Constraint>& cs) {
  using DST = tables::DescribeSliceTable;

  auto slice_id_fn = [](const Constraint& c) {
    return c.col_idx == static_cast<uint32_t>(DST::ColumnIndex::slice_id) &&
           c.op == FilterOp::kEq;
  };
  auto slice_id_it = std::find_if(cs.begin(), cs.end(), slice_id_fn);

  // We should always have valid iterators here because BestIndex should only
  // allow the constraint set to be chosen when we have an equality constraint
  // on both ts and upid.
  PERFETTO_CHECK(slice_id_it != cs.end());

  uint32_t slice_id_value = static_cast<uint32_t>(slice_id_it->value.AsLong());
  return DescribeSliceGenerator::InputValues{slice_id_value};
}

}  // namespace

DescribeSliceGenerator::DescribeSliceGenerator(TraceProcessorContext* context)
    : context_(context) {}

DescribeSliceGenerator::~DescribeSliceGenerator() = default;

util::Status DescribeSliceGenerator::ValidateConstraints(
    const QueryConstraints& qc) {
  using T = tables::DescribeSliceTable;

  const auto& cs = qc.constraints();

  auto slice_id_fn = [](const QueryConstraints::Constraint& c) {
    return c.column == static_cast<int>(T::ColumnIndex::slice_id) &&
           c.op == SQLITE_INDEX_CONSTRAINT_EQ;
  };
  bool has_slice_id_cs =
      std::find_if(cs.begin(), cs.end(), slice_id_fn) != cs.end();

  return has_slice_id_cs
             ? util::OkStatus()
             : util::ErrStatus("Failed to find required constraints");
}

std::unique_ptr<Table> DescribeSliceGenerator::ComputeTable(
    const std::vector<Constraint>& cs,
    const std::vector<Order>&) {
  auto input = GetDescribeSliceInputValues(cs);
  const auto& slices = context_->storage->slice_table();

  base::Optional<SliceDescription> opt_desc;
  auto status = DescribeSlice(slices, SliceId{input.slice_id_value}, &opt_desc);
  if (!status.ok())
    return nullptr;

  auto* pool = context_->storage->mutable_string_pool();
  std::unique_ptr<tables::DescribeSliceTable> table(
      new tables::DescribeSliceTable(pool, nullptr));

  if (opt_desc) {
    tables::DescribeSliceTable::Row row;
    row.description = context_->storage->InternString(
        base::StringView(opt_desc->description));
    row.doc_link =
        context_->storage->InternString(base::StringView(opt_desc->doc_link));
    row.slice_id = input.slice_id_value;
    table->Insert(row);
  }
  // We need to explicitly std::move as clang complains about a bug in old
  // compilers otherwise (-Wreturn-std-move-in-c++11).
  return std::move(table);
}

Table::Schema DescribeSliceGenerator::CreateSchema() {
  return tables::DescribeSliceTable::Schema();
}

std::string DescribeSliceGenerator::TableName() {
  return "describe_slice";
}

uint32_t DescribeSliceGenerator::EstimateRowCount() {
  return 1;
}

}  // namespace trace_processor
}  // namespace perfetto
