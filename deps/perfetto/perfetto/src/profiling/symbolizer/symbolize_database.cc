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

#include "src/profiling/symbolizer/symbolize_database.h"

#include <map>
#include <utility>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_splitter.h"

#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/trace_processor/trace_processor.h"

#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace profiling {

namespace {
using Iterator = trace_processor::TraceProcessor::Iterator;

constexpr const char* kQueryUnsymbolized =
    "select spm.name, spm.build_id, spf.rel_pc "
    "from stack_profile_frame spf "
    "join stack_profile_mapping spm "
    "on spf.mapping = spm.id "
    "where spm.build_id != '' and spf.symbol_set_id IS NULL";

using NameAndBuildIdPair = std::pair<std::string, std::string>;

std::string FromHex(const char* str, size_t size) {
  if (size % 2) {
    PERFETTO_DFATAL_OR_ELOG("Failed to parse hex %s", str);
    return "";
  }
  std::string result(size / 2, '\0');
  for (size_t i = 0; i < size; i += 2) {
    char hex_byte[3];
    hex_byte[0] = str[i];
    hex_byte[1] = str[i + 1];
    hex_byte[2] = '\0';
    char* end;
    long int byte = strtol(hex_byte, &end, 16);
    if (*end != '\0') {
      PERFETTO_DFATAL_OR_ELOG("Failed to parse hex %s", str);
      return "";
    }
    result[i / 2] = static_cast<char>(byte);
  }
  return result;
}

std::string FromHex(const std::string& str) {
  return FromHex(str.c_str(), str.size());
}

std::map<NameAndBuildIdPair, std::vector<uint64_t>> GetUnsymbolizedFrames(
    trace_processor::TraceProcessor* tp) {
  std::map<std::pair<std::string, std::string>, std::vector<uint64_t>> res;
  Iterator it = tp->ExecuteQuery(kQueryUnsymbolized);
  while (it.Next()) {
    auto name_and_buildid =
        std::make_pair(it.Get(0).AsString(), FromHex(it.Get(1).AsString()));
    int64_t rel_pc = it.Get(2).AsLong();
    res[name_and_buildid].emplace_back(rel_pc);
  }
  if (!it.Status().ok()) {
    PERFETTO_DFATAL_OR_ELOG("Invalid iterator: %s",
                            it.Status().message().c_str());
    return {};
  }
  return res;
}
}  // namespace

void SymbolizeDatabase(trace_processor::TraceProcessor* tp,
                       Symbolizer* symbolizer,
                       std::function<void(const std::string&)> callback) {
  PERFETTO_CHECK(symbolizer);
  auto unsymbolized = GetUnsymbolizedFrames(tp);
  for (auto it = unsymbolized.cbegin(); it != unsymbolized.cend(); ++it) {
    const auto& name_and_buildid = it->first;
    const std::vector<uint64_t>& rel_pcs = it->second;
    auto res = symbolizer->Symbolize(name_and_buildid.first,
                                     name_and_buildid.second, rel_pcs);
    if (res.empty())
      continue;

    protozero::HeapBuffered<perfetto::protos::pbzero::Trace> trace;
    auto* packet = trace->add_packet();
    auto* module_symbols = packet->set_module_symbols();
    module_symbols->set_path(name_and_buildid.first);
    module_symbols->set_build_id(name_and_buildid.second);
    PERFETTO_DCHECK(res.size() == rel_pcs.size());
    for (size_t i = 0; i < res.size(); ++i) {
      auto* address_symbols = module_symbols->add_address_symbols();
      address_symbols->set_address(rel_pcs[i]);
      for (const SymbolizedFrame& frame : res[i]) {
        auto* line = address_symbols->add_lines();
        line->set_function_name(frame.function_name);
        line->set_source_file_name(frame.file_name);
        line->set_line_number(frame.line);
      }
    }
    callback(trace.SerializeAsString());
  }
}

std::vector<std::string> GetPerfettoBinaryPath() {
  std::vector<std::string> roots;
  const char* root = getenv("PERFETTO_BINARY_PATH");
  if (root != nullptr) {
    for (base::StringSplitter sp(std::string(root), ':'); sp.Next();)
      roots.emplace_back(sp.cur_token(), sp.cur_token_size());
  }
  return roots;
}

}  // namespace profiling
}  // namespace perfetto
