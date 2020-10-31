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

#ifndef SRC_TRACED_PROBES_PACKAGES_LIST_PACKAGES_LIST_DATA_SOURCE_H_
#define SRC_TRACED_PROBES_PACKAGES_LIST_PACKAGES_LIST_DATA_SOURCE_H_

#include <functional>
#include <memory>
#include <set>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/scoped_file.h"

#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "protos/perfetto/config/android/packages_list_config.pbzero.h"
#include "protos/perfetto/trace/android/packages_list.pbzero.h"

#include "src/traced/probes/probes_data_source.h"

namespace perfetto {

class TraceWriter;

struct Package {
  std::string name;
  uint64_t uid = 0;
  bool debuggable = false;
  bool profileable_from_shell = false;
  int64_t version_code = 0;
};

bool ReadPackagesListLine(char* line, Package* package);
bool ParsePackagesListStream(protos::pbzero::PackagesList* packages_list,
                             const base::ScopedFstream& fs,
                             const std::set<std::string>& package_name_filter);

class PackagesListDataSource : public ProbesDataSource {
 public:
  static const ProbesDataSource::Descriptor descriptor;

  PackagesListDataSource(const DataSourceConfig& ds_config,
                         TracingSessionID session_id,
                         std::unique_ptr<TraceWriter> writer);
  // ProbesDataSource implementation.
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

  ~PackagesListDataSource() override;

 private:
  // If empty, include all package names. std::set over std::unordered_set as
  // this should be trivially small (or empty) in practice, and the latter uses
  // ever so slightly more memory.
  std::set<std::string> package_name_filter_;
  std::unique_ptr<TraceWriter> writer_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_PACKAGES_LIST_PACKAGES_LIST_DATA_SOURCE_H_
