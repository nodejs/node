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

#include "src/trace_processor/trace_processor_impl.h"

#include <inttypes.h>
#include <algorithm>

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/dynamic/describe_slice_generator.h"
#include "src/trace_processor/dynamic/experimental_counter_dur_generator.h"
#include "src/trace_processor/dynamic/experimental_flamegraph_generator.h"
#include "src/trace_processor/dynamic/experimental_slice_layout_generator.h"
#include "src/trace_processor/export_json.h"
#include "src/trace_processor/importers/additional_modules.h"
#include "src/trace_processor/importers/ftrace/sched_event_tracker.h"
#include "src/trace_processor/importers/fuchsia/fuchsia_trace_parser.h"
#include "src/trace_processor/importers/fuchsia/fuchsia_trace_tokenizer.h"
#include "src/trace_processor/importers/gzip/gzip_trace_parser.h"
#include "src/trace_processor/importers/json/json_trace_parser.h"
#include "src/trace_processor/importers/json/json_trace_tokenizer.h"
#include "src/trace_processor/importers/proto/metadata_tracker.h"
#include "src/trace_processor/importers/systrace/systrace_trace_parser.h"
#include "src/trace_processor/sqlite/span_join_operator_table.h"
#include "src/trace_processor/sqlite/sql_stats_table.h"
#include "src/trace_processor/sqlite/sqlite3_str_split.h"
#include "src/trace_processor/sqlite/sqlite_raw_table.h"
#include "src/trace_processor/sqlite/sqlite_table.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"
#include "src/trace_processor/sqlite/stats_table.h"
#include "src/trace_processor/sqlite/window_operator_table.h"
#include "src/trace_processor/types/variadic.h"

#include "src/trace_processor/metrics/metrics.descriptor.h"
#include "src/trace_processor/metrics/metrics.h"
#include "src/trace_processor/metrics/sql_metrics.h"

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <cxxabi.h>
#endif

// In Android and Chromium tree builds, we don't have the percentile module.
// Just don't include it.
#if PERFETTO_BUILDFLAG(PERFETTO_TP_PERCENTILE)
// defined in sqlite_src/ext/misc/percentile.c
extern "C" int sqlite3_percentile_init(sqlite3* db,
                                       char** error,
                                       const sqlite3_api_routines* api);
#endif  // PERFETTO_BUILDFLAG(PERFETTO_TP_PERCENTILE)

namespace perfetto {
namespace trace_processor {
namespace {

const char kAllTablesQuery[] =
    "SELECT tbl_name, type FROM (SELECT * FROM sqlite_master UNION ALL SELECT "
    "* FROM sqlite_temp_master)";

void InitializeSqlite(sqlite3* db) {
  char* error = nullptr;
  sqlite3_exec(db, "PRAGMA temp_store=2", 0, 0, &error);
  if (error) {
    PERFETTO_FATAL("Error setting pragma temp_store: %s", error);
  }
  sqlite3_str_split_init(db);
// In Android tree builds, we don't have the percentile module.
// Just don't include it.
#if PERFETTO_BUILDFLAG(PERFETTO_TP_PERCENTILE)
  sqlite3_percentile_init(db, &error, nullptr);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }
#endif
}

void BuildBoundsTable(sqlite3* db, std::pair<int64_t, int64_t> bounds) {
  char* error = nullptr;
  sqlite3_exec(db, "DELETE FROM trace_bounds", nullptr, nullptr, &error);
  if (error) {
    PERFETTO_ELOG("Error deleting from bounds table: %s", error);
    sqlite3_free(error);
    return;
  }

  char* insert_sql = sqlite3_mprintf("INSERT INTO trace_bounds VALUES(%" PRId64
                                     ", %" PRId64 ")",
                                     bounds.first, bounds.second);

  sqlite3_exec(db, insert_sql, 0, 0, &error);
  sqlite3_free(insert_sql);
  if (error) {
    PERFETTO_ELOG("Error inserting bounds table: %s", error);
    sqlite3_free(error);
  }
}

void CreateBuiltinTables(sqlite3* db) {
  char* error = nullptr;
  sqlite3_exec(db, "CREATE TABLE perfetto_tables(name STRING)", 0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }
  sqlite3_exec(db,
               "CREATE TABLE trace_bounds(start_ts BIG INT, end_ts BIG INT)", 0,
               0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  // Initialize the bounds table with some data so even before parsing any data,
  // we still have a valid table.
  BuildBoundsTable(db, std::make_pair(0, 0));
}

void CreateBuiltinViews(sqlite3* db) {
  char* error = nullptr;
  sqlite3_exec(db,
               "CREATE VIEW counter_definitions AS "
               "SELECT "
               "  *, "
               "  id AS counter_id "
               "FROM counter_track",
               0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  sqlite3_exec(db,
               "CREATE VIEW counter_values AS "
               "SELECT "
               "  *, "
               "  track_id as counter_id "
               "FROM counter",
               0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  sqlite3_exec(db,
               "CREATE VIEW counters AS "
               "SELECT * "
               "FROM counter_values v "
               "INNER JOIN counter_track t "
               "ON v.track_id = t.id "
               "ORDER BY ts;",
               0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  sqlite3_exec(db,
               "CREATE VIEW slice AS "
               "SELECT "
               "  *, "
               "  category AS cat, "
               "  id AS slice_id "
               "FROM internal_slice;",
               0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  sqlite3_exec(db,
               "CREATE VIEW instants AS "
               "SELECT "
               "*, "
               "0.0 as value "
               "FROM instant;",
               0, 0, &error);

  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  sqlite3_exec(db,
               "CREATE VIEW sched AS "
               "SELECT "
               "*, "
               "ts + dur as ts_end "
               "FROM sched_slice;",
               0, 0, &error);

  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  // Legacy view for "slice" table with a deprecated table name.
  // TODO(eseckler): Remove this view when all users have switched to "slice".
  sqlite3_exec(db,
               "CREATE VIEW slices AS "
               "SELECT * FROM slice;",
               0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  sqlite3_exec(db,
               "CREATE VIEW thread AS "
               "SELECT "
               "id as utid, "
               "* "
               "FROM internal_thread;",
               0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }

  sqlite3_exec(db,
               "CREATE VIEW process AS "
               "SELECT "
               "id as upid, "
               "* "
               "FROM internal_process;",
               0, 0, &error);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }
}

void ExportJson(sqlite3_context* ctx, int /*argc*/, sqlite3_value** argv) {
  TraceStorage* storage = static_cast<TraceStorage*>(sqlite3_user_data(ctx));
  FILE* output;
  if (sqlite3_value_type(argv[0]) == SQLITE_INTEGER) {
    // Assume input is an FD.
    output = fdopen(sqlite3_value_int(argv[0]), "w");
    if (!output) {
      sqlite3_result_error(ctx, "Couldn't open output file from given FD", -1);
      return;
    }
  } else {
    const char* filename =
        reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
    output = fopen(filename, "w");
    if (!output) {
      sqlite3_result_error(ctx, "Couldn't open output file", -1);
      return;
    }
  }

  util::Status result = json::ExportJson(storage, output);
  if (!result.ok()) {
    sqlite3_result_error(ctx, result.message().c_str(), -1);
    return;
  }
}

void CreateJsonExportFunction(TraceStorage* ts, sqlite3* db) {
  auto ret = sqlite3_create_function_v2(db, "EXPORT_JSON", 1, SQLITE_UTF8, ts,
                                        ExportJson, nullptr, nullptr,
                                        sqlite_utils::kSqliteStatic);
  if (ret) {
    PERFETTO_ELOG("Error initializing EXPORT_JSON");
  }
}

void Hash(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  base::Hash hash;
  for (int i = 0; i < argc; ++i) {
    sqlite3_value* value = argv[i];
    switch (sqlite3_value_type(value)) {
      case SQLITE_INTEGER:
        hash.Update(sqlite3_value_int64(value));
        break;
      case SQLITE_TEXT: {
        const char* ptr =
            reinterpret_cast<const char*>(sqlite3_value_text(value));
        hash.Update(ptr, strlen(ptr));
        break;
      }
      default:
        sqlite3_result_error(ctx, "Unsupported type of arg passed to HASH", -1);
        return;
    }
  }
  sqlite3_result_int64(ctx, static_cast<int64_t>(hash.digest()));
}

void Demangle(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  if (argc != 1) {
    sqlite3_result_error(ctx, "Unsupported number of arg passed to DEMANGLE",
                         -1);
    return;
  }
  sqlite3_value* value = argv[0];
  if (sqlite3_value_type(value) != SQLITE_TEXT) {
    sqlite3_result_error(ctx, "Unsupported type of arg passed to DEMANGLE", -1);
    return;
  }
  const char* ptr = reinterpret_cast<const char*>(sqlite3_value_text(value));
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  int ignored = 0;
  // This memory was allocated by malloc and will be passed to SQLite to free.
  char* demangled_name = abi::__cxa_demangle(ptr, nullptr, nullptr, &ignored);
  if (!demangled_name) {
    sqlite3_result_null(ctx);
    return;
  }
  sqlite3_result_text(ctx, demangled_name, -1, free);
#else
  sqlite3_result_text(ctx, ptr, -1, sqlite_utils::kSqliteTransient);
#endif
}

void LastNonNullStep(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  if (argc != 1) {
    sqlite3_result_error(
        ctx, "Unsupported number of args passed to LAST_NON_NULL", -1);
    return;
  }
  sqlite3_value* value = argv[0];
  if (sqlite3_value_type(value) == SQLITE_NULL) {
    return;
  }
  sqlite3_value** ptr = reinterpret_cast<sqlite3_value**>(
      sqlite3_aggregate_context(ctx, sizeof(sqlite3_value*)));
  if (ptr) {
    if (*ptr != nullptr) {
      sqlite3_value_free(*ptr);
    }
    *ptr = sqlite3_value_dup(value);
  }
}

void LastNonNullInverse(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  // Do nothing.
  base::ignore_result(ctx);
  base::ignore_result(argc);
  base::ignore_result(argv);
}

void LastNonNullValue(sqlite3_context* ctx) {
  sqlite3_value** ptr =
      reinterpret_cast<sqlite3_value**>(sqlite3_aggregate_context(ctx, 0));
  if (!ptr || !*ptr) {
    sqlite3_result_null(ctx);
  } else {
    sqlite3_result_value(ctx, *ptr);
  }
}

void LastNonNullFinal(sqlite3_context* ctx) {
  sqlite3_value** ptr =
      reinterpret_cast<sqlite3_value**>(sqlite3_aggregate_context(ctx, 0));
  if (!ptr || !*ptr) {
    sqlite3_result_null(ctx);
  } else {
    sqlite3_result_value(ctx, *ptr);
    sqlite3_value_free(*ptr);
  }
}

void CreateHashFunction(sqlite3* db) {
  auto ret = sqlite3_create_function_v2(
      db, "HASH", -1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, &Hash,
      nullptr, nullptr, nullptr);
  if (ret) {
    PERFETTO_ELOG("Error initializing HASH");
  }
}

void CreateDemangledNameFunction(sqlite3* db) {
  auto ret = sqlite3_create_function_v2(
      db, "DEMANGLE", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, &Demangle,
      nullptr, nullptr, nullptr);
  if (ret != SQLITE_OK) {
    PERFETTO_ELOG("Error initializing DEMANGLE: %s", sqlite3_errmsg(db));
  }
}

void CreateLastNonNullFunction(sqlite3* db) {
  auto ret = sqlite3_create_window_function(
      db, "LAST_NON_NULL", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
      &LastNonNullStep, &LastNonNullFinal, &LastNonNullValue,
      &LastNonNullInverse, nullptr);
  if (ret) {
    PERFETTO_ELOG("Error initializing LAST_NON_NULL");
  }
}

void ExtractArg(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  if (argc != 2) {
    sqlite3_result_error(ctx, "EXTRACT_ARG: 2 args required", -1);
    return;
  }
  if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER) {
    sqlite3_result_error(ctx, "EXTRACT_ARG: 1st argument should be arg set id",
                         -1);
    return;
  }
  if (sqlite3_value_type(argv[1]) != SQLITE_TEXT) {
    sqlite3_result_error(ctx, "EXTRACT_ARG: 2nd argument should be key", -1);
    return;
  }

  TraceStorage* storage = static_cast<TraceStorage*>(sqlite3_user_data(ctx));
  uint32_t arg_set_id = static_cast<uint32_t>(sqlite3_value_int(argv[0]));
  const char* key = reinterpret_cast<const char*>(sqlite3_value_text(argv[1]));

  const auto& args = storage->arg_table();
  RowMap filtered = args.FilterToRowMap(
      {args.arg_set_id().eq(arg_set_id), args.key().eq(key)});
  if (filtered.size() == 0) {
    sqlite3_result_null(ctx);
    return;
  }
  if (filtered.size() > 1) {
    sqlite3_result_error(
        ctx, "EXTRACT_ARG: received multiple args matching arg set id and key",
        -1);
  }

  uint32_t idx = filtered.Get(0);
  Variadic::Type type = *storage->GetVariadicTypeForId(args.value_type()[idx]);
  switch (type) {
    case Variadic::kBool:
    case Variadic::kInt:
    case Variadic::kUint:
    case Variadic::kPointer:
      sqlite3_result_int64(ctx, *args.int_value()[idx]);
      break;
    case Variadic::kJson:
    case Variadic::kString:
      sqlite3_result_text(ctx, args.string_value().GetString(idx).data(), -1,
                          nullptr);
      break;
    case Variadic::kReal:
      sqlite3_result_double(ctx, *args.real_value()[idx]);
      break;
  }
}

void CreateExtractArgFunction(TraceStorage* ts, sqlite3* db) {
  auto ret = sqlite3_create_function_v2(db, "EXTRACT_ARG", 2,
                                        SQLITE_UTF8 | SQLITE_DETERMINISTIC, ts,
                                        &ExtractArg, nullptr, nullptr, nullptr);
  if (ret != SQLITE_OK) {
    PERFETTO_FATAL("Error initializing EXTRACT_ARG: %s", sqlite3_errmsg(db));
  }
}

void SetupMetrics(TraceProcessor* tp,
                  sqlite3* db,
                  std::vector<metrics::SqlMetricFile>* sql_metrics) {
  tp->ExtendMetricsProto(kMetricsDescriptor.data(), kMetricsDescriptor.size());

  for (const auto& file_to_sql : metrics::sql_metrics::kFileToSql) {
    tp->RegisterMetric(file_to_sql.path, file_to_sql.sql);
  }

  {
    std::unique_ptr<metrics::RunMetricContext> ctx(
        new metrics::RunMetricContext());
    ctx->tp = tp;
    ctx->metrics = sql_metrics;
    auto ret = sqlite3_create_function_v2(
        db, "RUN_METRIC", -1, SQLITE_UTF8, ctx.release(), metrics::RunMetric,
        nullptr, nullptr,
        [](void* ptr) { delete static_cast<metrics::RunMetricContext*>(ptr); });
    if (ret)
      PERFETTO_ELOG("Error initializing RUN_METRIC");
  }

  {
    auto ret = sqlite3_create_function_v2(
        db, "RepeatedField", 1, SQLITE_UTF8, nullptr, nullptr,
        metrics::RepeatedFieldStep, metrics::RepeatedFieldFinal, nullptr);
    if (ret)
      PERFETTO_ELOG("Error initializing RepeatedField");
  }
}

void EnsureSqliteInitialized() {
  // sqlite3_initialize isn't actually thread-safe despite being documented
  // as such; we need to make sure multiple TraceProcessorImpl instances don't
  // call it concurrently and only gets called once per process, instead.
  static bool init_once = [] { return sqlite3_initialize() == SQLITE_OK; }();
  PERFETTO_CHECK(init_once);
}

}  // namespace

TraceProcessorImpl::TraceProcessorImpl(const Config& cfg)
    : TraceProcessorStorageImpl(cfg) {
  context_.fuchsia_trace_tokenizer.reset(new FuchsiaTraceTokenizer(&context_));
  context_.fuchsia_trace_parser.reset(new FuchsiaTraceParser(&context_));

  context_.systrace_trace_parser.reset(new SystraceTraceParser(&context_));

  if (gzip::IsGzipSupported())
    context_.gzip_trace_parser.reset(new GzipTraceParser(&context_));

  if (json::IsJsonSupported()) {
    context_.json_trace_tokenizer.reset(new JsonTraceTokenizer(&context_));
    context_.json_trace_parser.reset(new JsonTraceParser(&context_));
  }

  RegisterAdditionalModules(&context_);

  sqlite3* db = nullptr;
  EnsureSqliteInitialized();
  PERFETTO_CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
  InitializeSqlite(db);
  CreateBuiltinTables(db);
  CreateBuiltinViews(db);
  db_.reset(std::move(db));

  CreateJsonExportFunction(context_.storage.get(), db);
  CreateHashFunction(db);
  CreateDemangledNameFunction(db);
  CreateLastNonNullFunction(db);
  CreateExtractArgFunction(context_.storage.get(), db);

  SetupMetrics(this, *db_, &sql_metrics_);

  // Setup the query cache.
  query_cache_.reset(new QueryCache());

  const TraceStorage* storage = context_.storage.get();

  SqlStatsTable::RegisterTable(*db_, storage);
  StatsTable::RegisterTable(*db_, storage);

  // Operator tables.
  SpanJoinOperatorTable::RegisterTable(*db_, storage);
  WindowOperatorTable::RegisterTable(*db_, storage);

  // New style tables but with some custom logic.
  SqliteRawTable::RegisterTable(*db_, query_cache_.get(),
                                context_.storage.get());

  // Tables dynamically generated at query time.
  RegisterDynamicTable(std::unique_ptr<ExperimentalFlamegraphGenerator>(
      new ExperimentalFlamegraphGenerator(&context_)));
  RegisterDynamicTable(std::unique_ptr<ExperimentalCounterDurGenerator>(
      new ExperimentalCounterDurGenerator(storage->counter_table())));
  RegisterDynamicTable(std::unique_ptr<DescribeSliceGenerator>(
      new DescribeSliceGenerator(&context_)));
  RegisterDynamicTable(std::unique_ptr<ExperimentalSliceLayoutGenerator>(
      new ExperimentalSliceLayoutGenerator(
          context_.storage.get()->mutable_string_pool(),
          &storage->slice_table())));

  // New style db-backed tables.
  RegisterDbTable(storage->arg_table());
  RegisterDbTable(storage->thread_table());
  RegisterDbTable(storage->process_table());

  RegisterDbTable(storage->slice_table());
  RegisterDbTable(storage->sched_slice_table());
  RegisterDbTable(storage->instant_table());
  RegisterDbTable(storage->gpu_slice_table());

  RegisterDbTable(storage->track_table());
  RegisterDbTable(storage->thread_track_table());
  RegisterDbTable(storage->process_track_table());
  RegisterDbTable(storage->gpu_track_table());

  RegisterDbTable(storage->counter_table());

  RegisterDbTable(storage->counter_track_table());
  RegisterDbTable(storage->process_counter_track_table());
  RegisterDbTable(storage->thread_counter_track_table());
  RegisterDbTable(storage->cpu_counter_track_table());
  RegisterDbTable(storage->irq_counter_track_table());
  RegisterDbTable(storage->softirq_counter_track_table());
  RegisterDbTable(storage->gpu_counter_track_table());

  RegisterDbTable(storage->heap_graph_object_table());
  RegisterDbTable(storage->heap_graph_reference_table());
  RegisterDbTable(storage->heap_graph_class_table());

  RegisterDbTable(storage->symbol_table());
  RegisterDbTable(storage->heap_profile_allocation_table());
  RegisterDbTable(storage->cpu_profile_stack_sample_table());
  RegisterDbTable(storage->stack_profile_callsite_table());
  RegisterDbTable(storage->stack_profile_mapping_table());
  RegisterDbTable(storage->stack_profile_frame_table());
  RegisterDbTable(storage->package_list_table());
  RegisterDbTable(storage->profiler_smaps_table());

  RegisterDbTable(storage->android_log_table());

  RegisterDbTable(storage->vulkan_memory_allocations_table());

  RegisterDbTable(storage->graphics_frame_slice_table());
  RegisterDbTable(storage->graphics_frame_stats_table());

  RegisterDbTable(storage->metadata_table());
}

TraceProcessorImpl::~TraceProcessorImpl() {
  for (auto* it : iterators_)
    it->Reset();
}

util::Status TraceProcessorImpl::Parse(std::unique_ptr<uint8_t[]> data,
                                       size_t size) {
  bytes_parsed_ += size;
  return TraceProcessorStorageImpl::Parse(std::move(data), size);
}

std::string TraceProcessorImpl::GetCurrentTraceName() {
  if (current_trace_name_.empty())
    return "";
  auto size = " (" + std::to_string(bytes_parsed_ / 1024 / 1024) + " MB)";
  return current_trace_name_ + size;
}

void TraceProcessorImpl::SetCurrentTraceName(const std::string& name) {
  current_trace_name_ = name;
}

void TraceProcessorImpl::NotifyEndOfFile() {
  if (current_trace_name_.empty())
    current_trace_name_ = "Unnamed trace";

  TraceProcessorStorageImpl::NotifyEndOfFile();

  SchedEventTracker::GetOrCreate(&context_)->FlushPendingEvents();
  context_.metadata_tracker->SetMetadata(
      metadata::trace_size_bytes,
      Variadic::Integer(static_cast<int64_t>(bytes_parsed_)));
  BuildBoundsTable(*db_, context_.storage->GetTraceTimestampBoundsNs());

  // Create a snapshot of all tables and views created so far. This is so later
  // we can drop all extra tables created by the UI and reset to the original
  // state (see RestoreInitialTables).
  initial_tables_.clear();
  auto it = ExecuteQuery(kAllTablesQuery);
  while (it.Next()) {
    auto value = it.Get(0);
    PERFETTO_CHECK(value.type == SqlValue::Type::kString);
    initial_tables_.push_back(value.string_value);
  }
}

size_t TraceProcessorImpl::RestoreInitialTables() {
  std::vector<std::pair<std::string, std::string>> deletion_list;
  std::string msg = "Resetting DB to initial state, deleting table/views:";
  for (auto it = ExecuteQuery(kAllTablesQuery); it.Next();) {
    std::string name(it.Get(0).string_value);
    std::string type(it.Get(1).string_value);
    if (std::find(initial_tables_.begin(), initial_tables_.end(), name) ==
        initial_tables_.end()) {
      msg += " " + name;
      deletion_list.push_back(std::make_pair(type, name));
    }
  }

  PERFETTO_LOG("%s", msg.c_str());
  for (const auto& tn : deletion_list) {
    std::string query = "DROP " + tn.first + " " + tn.second;
    auto it = ExecuteQuery(query);
    while (it.Next()) {
    }
    // Index deletion can legitimately fail. If one creates an index "i" on a
    // table "t" but issues the deletion in the order (t, i), the DROP index i
    // will fail with "no such index" because deleting the table "t"
    // automatically deletes all associated indexes.
    if (!it.Status().ok() && tn.first != "index")
      PERFETTO_FATAL("%s -> %s", query.c_str(), it.Status().c_message());
  }
  return deletion_list.size();
}

TraceProcessor::Iterator TraceProcessorImpl::ExecuteQuery(
    const std::string& sql,
    int64_t time_queued) {
  sqlite3_stmt* raw_stmt;
  int err = sqlite3_prepare_v2(*db_, sql.c_str(), static_cast<int>(sql.size()),
                               &raw_stmt, nullptr);
  util::Status status;
  uint32_t col_count = 0;
  if (err != SQLITE_OK) {
    status = util::ErrStatus("%s", sqlite3_errmsg(*db_));
  } else {
    col_count = static_cast<uint32_t>(sqlite3_column_count(raw_stmt));
  }

  base::TimeNanos t_start = base::GetWallTimeNs();
  uint32_t sql_stats_row =
      context_.storage->mutable_sql_stats()->RecordQueryBegin(sql, time_queued,
                                                              t_start.count());

  std::unique_ptr<IteratorImpl> impl(new IteratorImpl(
      this, *db_, ScopedStmt(raw_stmt), col_count, status, sql_stats_row));
  iterators_.emplace_back(impl.get());
  return TraceProcessor::Iterator(std::move(impl));
}

void TraceProcessorImpl::InterruptQuery() {
  if (!db_)
    return;
  query_interrupted_.store(true);
  sqlite3_interrupt(db_.get());
}

util::Status TraceProcessorImpl::RegisterMetric(const std::string& path,
                                                const std::string& sql) {
  std::string stripped_sql;
  for (base::StringSplitter sp(sql, '\n'); sp.Next();) {
    if (strncmp(sp.cur_token(), "--", 2) != 0) {
      stripped_sql.append(sp.cur_token());
      stripped_sql.push_back('\n');
    }
  }

  // Check if the metric with the given path already exists and if it does, just
  // update the SQL associated with it.
  auto it = std::find_if(
      sql_metrics_.begin(), sql_metrics_.end(),
      [&path](const metrics::SqlMetricFile& m) { return m.path == path; });
  if (it != sql_metrics_.end()) {
    it->sql = stripped_sql;
    return util::OkStatus();
  }

  auto sep_idx = path.rfind("/");
  std::string basename =
      sep_idx == std::string::npos ? path : path.substr(sep_idx + 1);

  auto sql_idx = basename.rfind(".sql");
  if (sql_idx == std::string::npos) {
    return util::ErrStatus("Unable to find .sql extension for metric");
  }
  auto no_ext_name = basename.substr(0, sql_idx);

  metrics::SqlMetricFile metric;
  metric.path = path;
  metric.proto_field_name = no_ext_name;
  metric.output_table_name = no_ext_name + "_output";
  metric.sql = stripped_sql;
  sql_metrics_.emplace_back(metric);
  return util::OkStatus();
}

util::Status TraceProcessorImpl::ExtendMetricsProto(const uint8_t* data,
                                                    size_t size) {
  util::Status status = pool_.AddFromFileDescriptorSet(data, size);
  if (!status.ok())
    return status;

  for (const auto& desc : pool_.descriptors()) {
    // Convert the full name (e.g. .perfetto.protos.TraceMetrics.SubMetric)
    // into a function name of the form (TraceMetrics_SubMetric).
    auto fn_name = desc.full_name().substr(desc.package_name().size() + 1);
    std::replace(fn_name.begin(), fn_name.end(), '.', '_');

    std::unique_ptr<metrics::BuildProtoContext> ctx(
        new metrics::BuildProtoContext());
    ctx->tp = this;
    ctx->pool = &pool_;
    ctx->desc = &desc;

    auto ret = sqlite3_create_function_v2(
        *db_, fn_name.c_str(), -1, SQLITE_UTF8, ctx.release(),
        metrics::BuildProto, nullptr, nullptr, [](void* ptr) {
          delete static_cast<metrics::BuildProtoContext*>(ptr);
        });
    if (ret != SQLITE_OK)
      return util::ErrStatus("%s", sqlite3_errmsg(*db_));
  }
  return util::OkStatus();
}

util::Status TraceProcessorImpl::ComputeMetric(
    const std::vector<std::string>& metric_names,
    std::vector<uint8_t>* metrics_proto) {
  auto opt_idx = pool_.FindDescriptorIdx(".perfetto.protos.TraceMetrics");
  if (!opt_idx.has_value())
    return util::Status("Root metrics proto descriptor not found");

  const auto& root_descriptor = pool_.descriptors()[opt_idx.value()];
  return metrics::ComputeMetrics(this, metric_names, sql_metrics_,
                                 root_descriptor, metrics_proto);
}

TraceProcessor::IteratorImpl::IteratorImpl(TraceProcessorImpl* trace_processor,
                                           sqlite3* db,
                                           ScopedStmt stmt,
                                           uint32_t column_count,
                                           util::Status status,
                                           uint32_t sql_stats_row)
    : trace_processor_(trace_processor),
      db_(db),
      stmt_(std::move(stmt)),
      column_count_(column_count),
      status_(status),
      sql_stats_row_(sql_stats_row) {}

TraceProcessor::IteratorImpl::~IteratorImpl() {
  if (trace_processor_) {
    auto* its = &trace_processor_->iterators_;
    auto it = std::find(its->begin(), its->end(), this);
    PERFETTO_CHECK(it != its->end());
    its->erase(it);

    base::TimeNanos t_end = base::GetWallTimeNs();
    auto* sql_stats = trace_processor_->context_.storage->mutable_sql_stats();
    sql_stats->RecordQueryEnd(sql_stats_row_, t_end.count());
  }
}

void TraceProcessor::IteratorImpl::Reset() {
  *this = IteratorImpl(nullptr, nullptr, ScopedStmt(), 0,
                       util::ErrStatus("Trace processor was deleted"), 0);
}

void TraceProcessor::IteratorImpl::RecordFirstNextInSqlStats() {
  base::TimeNanos t_first_next = base::GetWallTimeNs();
  auto* sql_stats = trace_processor_->context_.storage->mutable_sql_stats();
  sql_stats->RecordQueryFirstNext(sql_stats_row_, t_first_next.count());
}

}  // namespace trace_processor
}  // namespace perfetto
