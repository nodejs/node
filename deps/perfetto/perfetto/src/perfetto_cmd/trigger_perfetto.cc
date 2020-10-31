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

#include <getopt.h>

#include <string>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/traced/traced.h"
#include "src/perfetto_cmd/trigger_producer.h"

namespace perfetto {
namespace {

int PrintUsage(const char* argv0) {
  PERFETTO_ELOG(R"(
Usage: %s TRIGGER...
  -h|--help  Show this message
)",
                argv0);
  return 1;
}

}  // namespace

int __attribute__((visibility("default")))
TriggerPerfettoMain(int argc, char** argv) {
  static const struct option long_options[] = {
      {"help", no_argument, nullptr, 'h'}, {nullptr, 0, nullptr, 0}};

  int option_index = 0;

  std::vector<std::string> triggers_to_activate;

  for (;;) {
    int option = getopt_long(argc, argv, "h", long_options, &option_index);

    if (option == 'h')
      return PrintUsage(argv[0]);

    if (option == -1)
      break;  // EOF.
  }

  for (int i = optind; i < argc; i++)
    triggers_to_activate.push_back(std::string(argv[i]));

  if (triggers_to_activate.size() == 0) {
    PERFETTO_ELOG("At least one trigger must the specified.");
    return PrintUsage(argv[0]);
  }

  bool finished_with_success = false;
  base::UnixTaskRunner task_runner;
  TriggerProducer producer(
      &task_runner,
      [&task_runner, &finished_with_success](bool success) {
        finished_with_success = success;
        task_runner.Quit();
      },
      &triggers_to_activate);
  task_runner.Run();

  return finished_with_success ? 0 : 1;
}

}  // namespace perfetto
