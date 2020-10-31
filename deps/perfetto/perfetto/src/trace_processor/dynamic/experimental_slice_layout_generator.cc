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

#include "src/trace_processor/dynamic/experimental_slice_layout_generator.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"

namespace perfetto {
namespace trace_processor {
namespace {

struct GroupInfo {
  GroupInfo(int64_t _start, int64_t _end, uint32_t _max_height)
      : start(_start), end(_end), max_height(_max_height) {}
  int64_t start;
  int64_t end;
  uint32_t max_height;
  uint32_t layout_depth;
};

}  // namespace

ExperimentalSliceLayoutGenerator::ExperimentalSliceLayoutGenerator(
    StringPool* string_pool,
    const tables::SliceTable* table)
    : string_pool_(string_pool),
      slice_table_(table),
      empty_string_id_(string_pool_->InternString("")) {}
ExperimentalSliceLayoutGenerator::~ExperimentalSliceLayoutGenerator() = default;

Table::Schema ExperimentalSliceLayoutGenerator::CreateSchema() {
  Table::Schema schema = tables::SliceTable::Schema();
  schema.columns.emplace_back(Table::Schema::Column{
      "layout_depth", SqlValue::Type::kLong, false /* is_id */,
      false /* is_sorted */, false /* is_hidden */});
  schema.columns.emplace_back(Table::Schema::Column{
      "filter_track_ids", SqlValue::Type::kString, false /* is_id */,
      false /* is_sorted */, true /* is_hidden */});
  return schema;
}

std::string ExperimentalSliceLayoutGenerator::TableName() {
  return "experimental_slice_layout";
}

uint32_t ExperimentalSliceLayoutGenerator::EstimateRowCount() {
  return slice_table_->row_count();
}

util::Status ExperimentalSliceLayoutGenerator::ValidateConstraints(
    const QueryConstraints& cs) {
  for (const auto& c : cs.constraints()) {
    if (c.column == kFilterTrackIdsColumnIndex && sqlite_utils::IsOpEq(c.op)) {
      return util::OkStatus();
    }
  }
  return util::ErrStatus(
      "experimental_slice_layout must have filter_track_ids constraint");
}

std::unique_ptr<Table> ExperimentalSliceLayoutGenerator::ComputeTable(
    const std::vector<Constraint>& cs,
    const std::vector<Order>&) {
  std::set<TrackId> selected_tracks;
  std::string filter_string = "";
  for (const auto& c : cs) {
    bool is_filter_track_ids = c.col_idx == kFilterTrackIdsColumnIndex;
    bool is_equal = c.op == FilterOp::kEq;
    bool is_string = c.value.type == SqlValue::kString;
    if (is_filter_track_ids && is_equal && is_string) {
      filter_string = c.value.AsString();
      for (base::StringSplitter sp(filter_string, ','); sp.Next();) {
        base::Optional<uint32_t> maybe = base::CStringToUInt32(sp.cur_token());
        if (maybe) {
          selected_tracks.insert(TrackId{maybe.value()});
        }
      }
    }
  }

  StringPool::Id filter_id =
      string_pool_->InternString(base::StringView(filter_string));
  return AddLayoutColumn(*slice_table_, selected_tracks, filter_id);
}

// The problem we're trying to solve is this: given a number of tracks each of
// which contain a number of 'stalactites' - depth 0 slices and all their
// children - layout the stalactites to minimize vertical depth without
// changing the horizontal (time) position. So given two tracks:
// Track A:
//     aaaaaaaaa       aaa
//                      aa
//                       a
// Track B:
//      bbb       bbb    bbb
//       b         b      b
// The result could be something like:
//     aaaaaaaaa  bbb  aaa
//                 b    aa
//      bbb              a
//       b
//                       bbb
//                        b
// We do this by computing an additional column: layout_depth. layout_depth
// tells us the vertical position of each slice in each stalactite.
//
// The algorithm works in three passes:
// 1. For each stalactite find the 'bounding box' (start, end, & max depth)
// 2. Considering each stalactite bounding box in start ts order pick a
//    layout_depth for the root slice of stalactite to avoid collisions with
//    all previous stalactite's we've considered.
// 3. Go though each slice and give it a layout_depth by summing it's
//    current depth and the root layout_depth of the stalactite it belongs to.
//
//
std::unique_ptr<Table> ExperimentalSliceLayoutGenerator::AddLayoutColumn(
    const Table& table,
    const std::set<TrackId>& selected,
    StringPool::Id filter_id) {
  const auto& track_id_col =
      *table.GetTypedColumnByName<tables::TrackTable::Id>("track_id");
  const auto& depth_col = *table.GetTypedColumnByName<uint32_t>("depth");
  const auto& ts_col = *table.GetTypedColumnByName<int64_t>("ts");
  const auto& dur_col = *table.GetTypedColumnByName<int64_t>("dur");

  std::map<TrackId, GroupInfo> groups;

  // Step 1:
  // Find the bounding box (start ts, end ts, and max depth) for each group
  // TODO(lalitm): Update this to use iterator (will be slow after event table)
  for (uint32_t i = 0; i < table.row_count(); ++i) {
    TrackId track_id = track_id_col[i];
    if (selected.count(track_id) == 0) {
      continue;
    }

    uint32_t depth = depth_col[i];
    int64_t start = ts_col[i];
    int64_t dur = dur_col[i];
    int64_t end = start + dur;

    std::map<TrackId, GroupInfo>::iterator it;
    bool inserted;
    std::tie(it, inserted) = groups.emplace(
        std::piecewise_construct, std::forward_as_tuple(track_id),
        std::forward_as_tuple(start, end, depth + 1));
    if (!inserted) {
      it->second.max_height = std::max(it->second.max_height, depth + 1);
      it->second.end = std::max(it->second.end, end);
    }
  }

  // Step 2:
  // Go though each group and choose a depth for the root slice.
  // We keep track of those groups where the start time has passed but the
  // end time has not in this vector:
  std::vector<GroupInfo*> still_open;
  for (auto& id_group : groups) {
    int64_t start = id_group.second.start;
    uint32_t max_height = id_group.second.max_height;

    // Discard all 'closed' groups where that groups end_ts is < our start_ts:
    {
      auto it = still_open.begin();
      while (it != still_open.end()) {
        if ((*it)->end < start) {
          it = still_open.erase(it);
        } else {
          ++it;
        }
      }
    }

    // Find a start layout depth for this group s.t. our start depth +
    // our max depth will not intersect with the start depth + max depth for
    // any of the open groups:
    uint32_t layout_depth = 0;
    bool done = false;
    while (!done) {
      done = true;
      uint32_t start_depth = layout_depth;
      uint32_t end_depth = layout_depth + max_height;
      for (const auto& open : still_open) {
        bool top = open->layout_depth <= start_depth &&
                   start_depth < open->layout_depth + open->max_height;
        bool bottom = open->layout_depth < end_depth &&
                      end_depth <= open->layout_depth + open->max_height;
        if (top || bottom) {
          // This is extremely dumb, we can make a much better guess for what
          // depth to try next but it is a little complicated to get right.
          layout_depth++;
          done = false;
          break;
        }
      }
    }

    // Add this group to the open groups & re
    still_open.push_back(&id_group.second);

    // Set our root layout depth:
    id_group.second.layout_depth = layout_depth;
  }

  // Step 3: Add the two new columns layout_depth and filter_track_ids:
  std::unique_ptr<SparseVector<int64_t>> layout_depth_column(
      new SparseVector<int64_t>());
  std::unique_ptr<SparseVector<StringPool::Id>> filter_column(
      new SparseVector<StringPool::Id>());

  for (uint32_t i = 0; i < table.row_count(); ++i) {
    TrackId track_id = track_id_col[i];
    uint32_t depth = depth_col[i];
    if (selected.count(track_id) == 0) {
      // Don't care about depth for slices from non-selected tracks:
      layout_depth_column->Append(0);
      // We (ab)use this column to also filter out all the slices we don't care
      // about by giving it a different value.
      filter_column->Append(empty_string_id_);
    } else {
      // Each slice depth is it's current slice depth + root slice depth of the
      // group:
      layout_depth_column->Append(depth + groups.at(track_id).layout_depth);
      // We must set this to the value we got in the constraint to ensure our
      // rows are not filtered out:
      filter_column->Append(filter_id);
    }
  }

  return std::unique_ptr<Table>(new Table(
      table
          .ExtendWithColumn("layout_depth", std::move(layout_depth_column),
                            TypedColumn<int64_t>::default_flags())
          .ExtendWithColumn("filter_track_ids", std::move(filter_column),
                            TypedColumn<StringPool::Id>::default_flags())));
}

}  // namespace trace_processor
}  // namespace perfetto
