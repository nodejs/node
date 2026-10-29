#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "libplatform/libplatform.h"
#include "node_internals.h"
#include "node_snapshot_builder.h"
#include "util-inl.h"
#include "v8.h"

int BuildSnapshot(int argc, char* argv[]);

#ifdef _WIN32
#include <windows.h>

int wmain(int argc, wchar_t* wargv[]) {
  // Windows needs conversion from wchar_t to char.

  // Convert argv to UTF8.
  char** argv = new char*[argc + 1];
  for (int i = 0; i < argc; i++) {
    // Compute the size of the required buffer
    DWORD size = WideCharToMultiByte(
        CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) {
      // This should never happen.
      fprintf(stderr, "Could not convert arguments to utf8.");
      exit(1);
    }
    // Do the actual conversion
    argv[i] = new char[size];
    DWORD result = WideCharToMultiByte(
        CP_UTF8, 0, wargv[i], -1, argv[i], size, nullptr, nullptr);
    if (result == 0) {
      // This should never happen.
      fprintf(stderr, "Could not convert arguments to utf8.");
      exit(1);
    }
  }
  argv[argc] = nullptr;
#else   // UNIX
int main(int argc, char* argv[]) {
  argv = uv_setup_args(argc, argv);

  // Disable stdio buffering, it interacts poorly with printf()
  // calls elsewhere in the program (e.g., any logging from V8.)
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
#endif  // _WIN32

  return BuildSnapshot(argc, argv);
}

static const char kBaseBlobFlag[] = "--v8-snapshot-blob=";

int BuildSnapshot(int argc, char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);
  // --v8-snapshot-blob=<file>: build on top of this V8 startup blob (for
  // hosts whose V8 uses external startup data) instead of from scratch.
  std::string base_blob_bytes;
  v8::StartupData base_blob{nullptr, 0};
  for (auto it = args.begin(); it != args.end(); ++it) {
    if (it->starts_with(kBaseBlobFlag)) {
      std::string path = it->substr(sizeof(kBaseBlobFlag) - 1);
      args.erase(it);
      if (node::ReadFileSync(path.c_str(), &base_blob_bytes) != 0) {
        std::cerr << "Cannot read V8 snapshot blob " << path << "\n";
        return 1;
      }
      base_blob = {base_blob_bytes.data(),
                   static_cast<int>(base_blob_bytes.size())};
      break;
    }
  }

  if (args.size() < 2) {
    std::cerr
        << "Usage: " << argv[0]
        << " [--v8-snapshot-blob=<path/to/blob.bin>] <path/to/output.cc>\n";
    std::cerr << "       " << argv[0]
              << " [--v8-snapshot-blob=<path/to/blob.bin>] --build-snapshot "
              << "<path/to/script.js> <path/to/output.cc>\n";
    return 1;
  }

  std::shared_ptr<node::InitializationResult> result =
      node::InitializeOncePerProcess(
          args, node::ProcessInitializationFlags::kGeneratePredictableSnapshot);

  if (result->exit_code() != 0) {
    for (const std::string& error : result->errors()) {
      std::cerr << error << "\n";
    }
    std::cerr << "node_mksnapshot failed with exit code " << result->exit_code()
              << "\n";
    return static_cast<int>(result->exit_code());
  }

  CHECK(!result->early_return());

  std::string out_path;
  std::optional<std::string_view> builder_script_path = std::nullopt;
  if (node::per_process::cli_options->per_isolate->build_snapshot) {
    builder_script_path = result->args()[1];
    out_path = result->args()[2];
  } else {
    out_path = result->args()[1];
  }

#ifdef NODE_MKSNAPSHOT_USE_ARRAY_LITERALS
  bool use_array_literals = true;
#else
  bool use_array_literals = false;
#endif

  node::SnapshotConfig snapshot_config;
  snapshot_config.builder_script_path = builder_script_path;
  if (base_blob.data != nullptr) snapshot_config.base_blob = &base_blob;

#ifdef NODE_USE_NODE_CODE_CACHE
  snapshot_config.flags = node::SnapshotFlags::kDefault;
#else
  snapshot_config.flags = node::SnapshotFlags::kWithoutCodeCache;
#endif

  node::ExitCode exit_code =
      node::SnapshotBuilder::GenerateAsSource(out_path.c_str(),
                                              result->args(),
                                              result->exec_args(),
                                              snapshot_config,
                                              use_array_literals);

  node::TearDownOncePerProcess();
  return static_cast<int>(exit_code);
}
