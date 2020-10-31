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

#ifndef SRC_TRACE_PROCESSOR_SQLITE_QUERY_CACHE_H_
#define SRC_TRACE_PROCESSOR_SQLITE_QUERY_CACHE_H_

#include "perfetto/ext/base/optional.h"

#include "src/trace_processor/db/table.h"
#include "src/trace_processor/sqlite/query_constraints.h"

namespace perfetto {
namespace trace_processor {

// Implements a simple caching strategy for commonly executed queries.
// TODO(lalitm): the design of this class is very experimental. It was mainly
// introduced to solve a specific problem (slow process summary tracks in the
// Perfetto UI) and should not be modified without a full design discussion.
class QueryCache {
 public:
  using Constraint = QueryConstraints::Constraint;

  // Returns a cached table if the passed query set are currenly cached or
  // nullptr otherwise.
  std::shared_ptr<Table> GetIfCached(const Table* source,
                                     const std::vector<Constraint>& cs) const {
    if (cached_.source != source || cs.size() != cached_.constraints.size())
      return nullptr;

    auto p = [](const Constraint& a, const Constraint& b) {
      return a.column == b.column && a.op == b.op;
    };
    bool same_cs =
        std::equal(cs.begin(), cs.end(), cached_.constraints.begin(), p);
    return same_cs ? cached_.table : nullptr;
  }

  // Caches the table with the given source, constraint and order set. Returns
  // a pointer to the newly cached table.
  std::shared_ptr<Table> GetOrCache(
      const Table* source,
      const std::vector<QueryConstraints::Constraint>& cs,
      std::function<Table()> fn) {
    std::shared_ptr<Table> cached = GetIfCached(source, cs);
    if (cached)
      return cached;

    cached_.source = source;
    cached_.constraints = cs;
    cached_.table.reset(new Table(fn()));
    return cached_.table;
  }

 private:
  struct CachedTable {
    std::shared_ptr<Table> table;

    const Table* source = nullptr;
    std::vector<Constraint> constraints;
  };

  CachedTable cached_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SQLITE_QUERY_CACHE_H_
