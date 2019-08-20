#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "libplatform/libplatform.h"
#include "node_internals.h"
#include "snapshot_builder.h"
#include "util-inl.h"
#include "v8.h"

#ifdef _WIN32
#include <windows.h>

int wmain(int argc, wchar_t* argv[]) {
#else   // UNIX
int main(int argc, char* argv[]) {
#endif  // _WIN32

  v8::V8::SetFlagsFromString("--random_seed=42");

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path/to/output.cc>\n";
    return 1;
  }

  std::ofstream out;
  out.open(argv[1], std::ios::out | std::ios::binary);
  if (!out.is_open()) {
    std::cerr << "Cannot open " << argv[1] << "\n";
    return 1;
  }

  int node_argc = 1;
  char argv0[] = "node";
  char* node_argv[] = {argv0, nullptr};

  node::InitializationResult result =
      node::InitializeOncePerProcess(node_argc, node_argv);
  CHECK(!result.early_return);
  CHECK_EQ(result.exit_code, 0);

  {
    std::string snapshot =
        node::SnapshotBuilder::Generate(result.args, result.exec_args);
    out << snapshot;
    out.close();
  }

  node::TearDownOncePerProcess();
  return 0;
}
