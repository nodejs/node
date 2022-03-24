#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "libplatform/libplatform.h"
#include "node_internals.h"
#include "node_snapshotable.h"
#include "util-inl.h"
#include "v8.h"

#ifdef _WIN32
#include <windows.h>

int wmain(int argc, wchar_t* argv[]) {
#else   // UNIX
int main(int argc, char* argv[]) {
  argv = uv_setup_args(argc, argv);
#endif  // _WIN32

  v8::V8::SetFlagsFromString("--random_seed=42");

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path/to/output.cc>\n";
    std::cerr << "       " << argv[0] << " --build-snapshot "
              << "<path/to/script.js> <path/to/output.cc>\n";
    return 1;
  }

// Windows needs conversion from wchar_t to char. See node_main.cc
#ifdef _WIN32
  int node_argc = 1;
  char argv0[] = "node";
  char* node_argv[] = {argv0, nullptr};
  node::InitializationResult result =
      node::InitializeOncePerProcess(node_argc, node_argv);
#else
  node::InitializationResult result =
      node::InitializeOncePerProcess(argc, argv);
#endif

  CHECK(!result.early_return);
  CHECK_EQ(result.exit_code, 0);

  std::string snapshot_main;
  std::string out_path;
  if (node::per_process::cli_options->build_snapshot) {
    snapshot_main = result.args[1];
    out_path = result.args[2];
  } else {
    out_path = result.args[1];
  }

  std::ofstream out(out_path, std::ios::out | std::ios::binary);
  if (!out) {
    std::cerr << "Cannot open " << out_path << "\n";
    return 1;
  }

  {
    std::string snapshot = node::SnapshotBuilder::Generate(
        snapshot_main, result.args, result.exec_args);
    out << snapshot;

    if (!out) {
      std::cerr << "Failed to write " << out_path << "\n";
      out.close();
      return 1;
    }

    out.close();
  }

  node::TearDownOncePerProcess();
  return 0;
}
