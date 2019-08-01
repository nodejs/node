// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>
#include <sstream>

#include "src/torque/ls/globals.h"
#include "src/torque/ls/message-handler.h"
#include "src/torque/ls/message-pipe.h"
#include "src/torque/server-data.h"
#include "src/torque/source-positions.h"

namespace v8 {
namespace internal {
namespace torque {
namespace ls {

int WrappedMain(int argc, const char** argv) {
  Logger::Scope log_scope;
  TorqueFileList::Scope files_scope;
  LanguageServerData::Scope server_data_scope;
  SourceFileMap::Scope source_file_map_scope;
  DiagnosticsFiles::Scope diagnostics_files_scope;

  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-l", argv[i])) {
      Logger::Enable(argv[++i]);
      break;
    }
  }

  while (true) {
    auto message = ReadMessage();

    // TODO(szuend): We should probably offload the actual message handling
    //               (even the parsing) to a background thread, so we can
    //               keep receiving messages. We might also receive
    //               $/cancelRequests or contet updates, that require restarts.
    HandleMessage(message, &WriteMessage);
  }
  return 0;
}

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8

int main(int argc, const char** argv) {
  return v8::internal::torque::ls::WrappedMain(argc, argv);
}
