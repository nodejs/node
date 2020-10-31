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

#ifndef SRC_TRACE_PROCESSOR_SQLITE_SCOPED_DB_H_
#define SRC_TRACE_PROCESSOR_SQLITE_SCOPED_DB_H_

#include <sqlite3.h>

#include "perfetto/ext/base/scoped_file.h"

extern "C" {
struct sqlite3;
struct sqlite3_stmt;
SQLITE_API extern int sqlite3_close(sqlite3*);
SQLITE_API extern int sqlite3_finalize(sqlite3_stmt* pStmt);
}

namespace perfetto {
namespace trace_processor {

using ScopedDb = base::ScopedResource<sqlite3*, sqlite3_close, nullptr>;
using ScopedStmt = base::ScopedResource<sqlite3_stmt*,
                                        sqlite3_finalize,
                                        nullptr,
                                        /*CheckClose=*/false>;

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SQLITE_SCOPED_DB_H_
