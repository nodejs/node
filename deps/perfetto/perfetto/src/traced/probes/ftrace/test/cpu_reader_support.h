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

#ifndef SRC_TRACED_PROBES_FTRACE_TEST_CPU_READER_SUPPORT_H_
#define SRC_TRACED_PROBES_FTRACE_TEST_CPU_READER_SUPPORT_H_

#include <memory>
#include <string>

#include "src/traced/probes/ftrace/proto_translation_table.h"

namespace perfetto {

struct ExamplePage {
  // The name of the format file set used in the collection of this example
  // page. Should name a directory under src/traced/probes/ftrace/test/data.
  const char* name;
  // The non-zero prefix of xxd'ing the page.
  const char* data;
};

// Create a ProtoTranslationTable using the fomat files in
// directory |name|. Caches the table for subsequent lookups.
ProtoTranslationTable* GetTable(const std::string& name);

// Convert xxd output into binary data.
std::unique_ptr<uint8_t[]> PageFromXxd(const std::string& text);

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_TEST_CPU_READER_SUPPORT_H_
