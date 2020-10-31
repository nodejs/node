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

#include "src/trace_processor/dynamic/experimental_flamegraph_generator.h"

#include "perfetto/ext/base/string_utils.h"

#include "src/trace_processor/importers/proto/heap_graph_tracker.h"
#include "src/trace_processor/importers/proto/heap_profile_tracker.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

namespace {

ExperimentalFlamegraphGenerator::InputValues GetFlamegraphInputValues(
    const std::vector<Constraint>& cs) {
  using T = tables::ExperimentalFlamegraphNodesTable;

  auto ts_fn = [](const Constraint& c) {
    return c.col_idx == static_cast<uint32_t>(T::ColumnIndex::ts) &&
           c.op == FilterOp::kEq;
  };
  auto upid_fn = [](const Constraint& c) {
    return c.col_idx == static_cast<uint32_t>(T::ColumnIndex::upid) &&
           c.op == FilterOp::kEq;
  };
  auto profile_type_fn = [](const Constraint& c) {
    return c.col_idx == static_cast<uint32_t>(T::ColumnIndex::profile_type) &&
           c.op == FilterOp::kEq;
  };
  auto focus_str_fn = [](const Constraint& c) {
    return c.col_idx == static_cast<uint32_t>(T::ColumnIndex::focus_str) &&
           c.op == FilterOp::kEq;
  };

  auto ts_it = std::find_if(cs.begin(), cs.end(), ts_fn);
  auto upid_it = std::find_if(cs.begin(), cs.end(), upid_fn);
  auto profile_type_it = std::find_if(cs.begin(), cs.end(), profile_type_fn);
  auto focus_str_it = std::find_if(cs.begin(), cs.end(), focus_str_fn);

  // We should always have valid iterators here because BestIndex should only
  // allow the constraint set to be chosen when we have an equality constraint
  // on both ts and upid.
  PERFETTO_CHECK(ts_it != cs.end());
  PERFETTO_CHECK(upid_it != cs.end());
  PERFETTO_CHECK(profile_type_it != cs.end());

  int64_t ts = ts_it->value.AsLong();
  UniquePid upid = static_cast<UniquePid>(upid_it->value.AsLong());
  std::string profile_type = profile_type_it->value.AsString();
  std::string focus_str =
      focus_str_it != cs.end() ? focus_str_it->value.AsString() : "";
  return ExperimentalFlamegraphGenerator::InputValues{ts, upid, profile_type,
                                                      focus_str};
}

class Matcher {
 public:
  explicit Matcher(const std::string& str) : focus_str_(base::ToLower(str)) {}
  Matcher(const Matcher&) = delete;
  Matcher& operator=(const Matcher&) = delete;

  bool matches(const std::string& s) const {
    // TODO(149833691): change to regex.
    // We cannot use regex.h (does not exist in windows) or std regex (throws
    // exceptions).
    return base::Contains(base::ToLower(s), focus_str_);
  }

 private:
  const std::string focus_str_;
};

enum class FocusedState {
  kNotFocused,
  kFocusedPropagating,
  kFocusedNotPropagating,
};

using tables::ExperimentalFlamegraphNodesTable;
std::vector<FocusedState> ComputeFocusedState(
    const ExperimentalFlamegraphNodesTable& table,
    const Matcher& focus_matcher) {
  // Each row corresponds to a node in the flame chart tree with its parent
  // ptr. Root trees (no parents) will have a null parent ptr.
  std::vector<FocusedState> focused(table.row_count());

  for (uint32_t i = 0; i < table.row_count(); ++i) {
    auto parent_id = table.parent_id()[i];
    // Constraint: all descendants MUST come after their parents.
    PERFETTO_DCHECK(!parent_id.has_value() || *parent_id < table.id()[i]);

    if (focus_matcher.matches(table.name().GetString(i).ToStdString())) {
      // Mark as focused
      focused[i] = FocusedState::kFocusedPropagating;
      auto current = parent_id;
      // Mark all parent nodes as focused
      while (current.has_value()) {
        auto current_idx = *table.id().IndexOf(*current);
        if (focused[current_idx] != FocusedState::kNotFocused) {
          // We have already visited these nodes, skip
          break;
        }
        focused[current_idx] = FocusedState::kFocusedNotPropagating;
        current = table.parent_id()[current_idx];
      }
    } else if (parent_id.has_value() &&
               focused[*table.id().IndexOf(*parent_id)] ==
                   FocusedState::kFocusedPropagating) {
      // Focus cascades downwards.
      focused[i] = FocusedState::kFocusedPropagating;
    } else {
      focused[i] = FocusedState::kNotFocused;
    }
  }
  return focused;
}

struct CumulativeCounts {
  int64_t size;
  int64_t count;
  int64_t alloc_size;
  int64_t alloc_count;
};
std::unique_ptr<tables::ExperimentalFlamegraphNodesTable> FocusTable(
    TraceStorage* storage,
    std::unique_ptr<ExperimentalFlamegraphNodesTable> in,
    const std::string& focus_str) {
  if (in->row_count() == 0 || focus_str.empty()) {
    return in;
  }
  std::vector<FocusedState> focused_state =
      ComputeFocusedState(*in.get(), Matcher(focus_str));
  std::unique_ptr<ExperimentalFlamegraphNodesTable> tbl(
      new tables::ExperimentalFlamegraphNodesTable(
          storage->mutable_string_pool(), nullptr));

  // Recompute cumulative counts
  std::vector<CumulativeCounts> node_to_cumulatives(in->row_count());
  for (int64_t idx = in->row_count() - 1; idx >= 0; --idx) {
    auto i = static_cast<uint32_t>(idx);
    if (focused_state[i] == FocusedState::kNotFocused) {
      continue;
    }
    auto& cumulatives = node_to_cumulatives[i];
    cumulatives.size += in->size()[i];
    cumulatives.count += in->count()[i];
    cumulatives.alloc_size += in->alloc_size()[i];
    cumulatives.alloc_count += in->alloc_count()[i];

    auto parent_id = in->parent_id()[i];
    if (parent_id.has_value()) {
      auto& parent_cumulatives =
          node_to_cumulatives[*in->id().IndexOf(*parent_id)];
      parent_cumulatives.size += cumulatives.size;
      parent_cumulatives.count += cumulatives.count;
      parent_cumulatives.alloc_size += cumulatives.alloc_size;
      parent_cumulatives.alloc_count += cumulatives.alloc_count;
    }
  }

  // Mapping between the old rows ('node') to the new identifiers.
  std::vector<ExperimentalFlamegraphNodesTable::Id> node_to_id(in->row_count());
  for (uint32_t i = 0; i < in->row_count(); ++i) {
    if (focused_state[i] == FocusedState::kNotFocused) {
      continue;
    }

    tables::ExperimentalFlamegraphNodesTable::Row alloc_row{};
    // We must reparent the rows as every insertion will get its own
    // identifier.
    auto original_parent_id = in->parent_id()[i];
    if (original_parent_id.has_value()) {
      auto original_idx = *in->id().IndexOf(*original_parent_id);
      alloc_row.parent_id = node_to_id[original_idx];
    }

    alloc_row.ts = in->ts()[i];
    alloc_row.upid = in->upid()[i];
    alloc_row.profile_type = in->profile_type()[i];
    alloc_row.depth = in->depth()[i];
    alloc_row.name = in->name()[i];
    alloc_row.map_name = in->map_name()[i];
    alloc_row.count = in->count()[i];
    alloc_row.size = in->size()[i];
    alloc_row.alloc_count = in->alloc_count()[i];
    alloc_row.alloc_size = in->alloc_size()[i];

    const auto& cumulative = node_to_cumulatives[i];
    alloc_row.cumulative_count = cumulative.count;
    alloc_row.cumulative_size = cumulative.size;
    alloc_row.cumulative_alloc_count = cumulative.alloc_count;
    alloc_row.cumulative_alloc_size = cumulative.alloc_size;
    node_to_id[i] = tbl->Insert(alloc_row).id;
  }
  return tbl;
}
}  // namespace

ExperimentalFlamegraphGenerator::ExperimentalFlamegraphGenerator(
    TraceProcessorContext* context)
    : context_(context) {}

ExperimentalFlamegraphGenerator::~ExperimentalFlamegraphGenerator() = default;

util::Status ExperimentalFlamegraphGenerator::ValidateConstraints(
    const QueryConstraints& qc) {
  using T = tables::ExperimentalFlamegraphNodesTable;

  const auto& cs = qc.constraints();

  auto ts_fn = [](const QueryConstraints::Constraint& c) {
    return c.column == static_cast<int>(T::ColumnIndex::ts) &&
           c.op == SQLITE_INDEX_CONSTRAINT_EQ;
  };
  bool has_ts_cs = std::find_if(cs.begin(), cs.end(), ts_fn) != cs.end();

  auto upid_fn = [](const QueryConstraints::Constraint& c) {
    return c.column == static_cast<int>(T::ColumnIndex::upid) &&
           c.op == SQLITE_INDEX_CONSTRAINT_EQ;
  };
  bool has_upid_cs = std::find_if(cs.begin(), cs.end(), upid_fn) != cs.end();

  auto profile_type_fn = [](const QueryConstraints::Constraint& c) {
    return c.column == static_cast<int>(T::ColumnIndex::profile_type) &&
           c.op == SQLITE_INDEX_CONSTRAINT_EQ;
  };
  bool has_profile_type_cs =
      std::find_if(cs.begin(), cs.end(), profile_type_fn) != cs.end();

  return has_ts_cs && has_upid_cs && has_profile_type_cs
             ? util::OkStatus()
             : util::ErrStatus("Failed to find required constraints");
}

std::unique_ptr<Table> ExperimentalFlamegraphGenerator::ComputeTable(
    const std::vector<Constraint>& cs,
    const std::vector<Order>&) {
  // Get the input column values and compute the flamegraph using them.
  auto values = GetFlamegraphInputValues(cs);

  std::unique_ptr<tables::ExperimentalFlamegraphNodesTable> table;
  if (values.profile_type == "graph") {
    auto* tracker = HeapGraphTracker::GetOrCreate(context_);
    table = tracker->BuildFlamegraph(values.ts, values.upid);
  }
  if (values.profile_type == "native") {
    table =
        BuildNativeFlamegraph(context_->storage.get(), values.upid, values.ts);
  }
  if (!values.focus_str.empty()) {
    table =
        FocusTable(context_->storage.get(), std::move(table), values.focus_str);
    // The pseudocolumns must be populated because as far as SQLite is
    // concerned these are equality constraints.
    auto focus_id =
        context_->storage->InternString(base::StringView(values.focus_str));
    for (uint32_t i = 0; i < table->row_count(); ++i) {
      table->mutable_focus_str()->Set(i, focus_id);
    }
  }
  // We need to explicitly std::move as clang complains about a bug in old
  // compilers otherwise (-Wreturn-std-move-in-c++11).
  return std::move(table);
}

Table::Schema ExperimentalFlamegraphGenerator::CreateSchema() {
  return tables::ExperimentalFlamegraphNodesTable::Schema();
}

std::string ExperimentalFlamegraphGenerator::TableName() {
  return "experimental_flamegraph";
}

uint32_t ExperimentalFlamegraphGenerator::EstimateRowCount() {
  // TODO(lalitm): return a better estimate here when possible.
  return 1024;
}

}  // namespace trace_processor
}  // namespace perfetto
