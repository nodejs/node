#include "embedtest_node_api.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <optional>

class CStringArray {
 public:
  explicit CStringArray(const std::vector<std::string>& strings) noexcept
      : size_(strings.size()) {
    if (size_ < inplace_buffer_.size()) {
      cstrings_ = inplace_buffer_.data();
    } else {
      allocated_buffer_ = std::make_unique<const char*[]>(size_);
      cstrings_ = allocated_buffer_.get();
    }
    for (size_t i = 0; i < size_; ++i) {
      cstrings_[i] = strings[i].c_str();
    }
  }

  const char** cstrings() const { return cstrings_; }
  size_t size() const { return size_; }

 private:
  const char** cstrings_;
  size_t size_;
  std::array<const char*, 32> inplace_buffer_;
  std::unique_ptr<const char*[]> allocated_buffer_;
};

static int32_t RunNodeInstance();

extern "C" int32_t test_main_snapshot_node_api(int32_t argc, char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);
  bool early_return = false;
  int32_t exit_code = 0;

  std::vector<std::string> errors;
  node_api_initialize_platform(argc,
                               argv,
                               node_api_platform_no_flags,
                               GetMessageVector,
                               &errors,
                               &early_return,
                               &exit_code);
  if (early_return) {
    if (exit_code != 0) {
      for (const std::string& err : errors)
        fprintf(stderr, "%s: %s\n", args[0].c_str(), err.c_str());
    } else {
      for (const std::string& err : errors) printf("%s\n", err.c_str());
    }
    return exit_code;
  }

  CHECK_EXIT_CODE(RunNodeInstance());

  CHECK(node_api_dispose_platform());
  return 0;
}

static const char* exe_name;

static void NAPI_CDECL get_errors(void* data,
                                  const char* errors[],
                                  size_t count) {
  for (size_t i = 0; i < count && i < 30; ++i) {
    fprintf(stderr, "%s: %s\n", exe_name, errors[i]);
  }
}

int32_t RunNodeInstance() {
  // Format of the arguments of this binary:
  // Building snapshot:
  // embedtest js_code_to_eval arg1 arg2...
  //           --embedder-snapshot-blob blob-path
  //           --embedder-snapshot-create
  //           [--embedder-snapshot-as-file]
  //           [--without-code-cache]
  // Running snapshot:
  // embedtest --embedder-snapshot-blob blob-path
  //           [--embedder-snapshot-as-file]
  //           arg1 arg2...
  // No snapshot:
  // embedtest arg1 arg2...

  int32_t exit_code = 0;

  node_api_env_options options;
  CHECK(node_api_create_env_options(&options));
  std::vector<std::string> args, exec_args;
  CHECK(node_api_env_options_get_args(options, GetArgsVector, &args));
  CHECK(node_api_env_options_get_exec_args(options, GetArgsVector, &exec_args));

  exe_name = args[0].c_str();
  std::vector<std::string> filtered_args;
  bool is_building_snapshot = false;
  node_api_snapshot_flags snapshot_flags = node_api_snapshot_no_flags;
  std::string snapshot_blob_path;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--embedder-snapshot-create") {
      is_building_snapshot = true;
    } else if (arg == "--embedder-snapshot-as-file") {
      // This parameter is not implemented by the Node-API, and we must not
      // include it in the filtered_args.
    } else if (arg == "--without-code-cache") {
      snapshot_flags = static_cast<node_api_snapshot_flags>(
          snapshot_flags | node_api_snapshot_no_code_cache);
    } else if (arg == "--embedder-snapshot-blob") {
      assert(i + 1 < args.size());
      snapshot_blob_path = args[i + 1];
      i++;
    } else {
      filtered_args.push_back(arg);
    }
  }

  bool use_snapshot = false;
  if (!snapshot_blob_path.empty() && !is_building_snapshot) {
    use_snapshot = true;
    FILE* fp = fopen(snapshot_blob_path.c_str(), "rb");
    assert(fp != nullptr);
    // Node-API only supports loading snapshots from blobs.
    uv_fs_t req = uv_fs_t();
    int32_t statret =
        uv_fs_stat(nullptr, &req, snapshot_blob_path.c_str(), nullptr);
    assert(statret == 0);
    size_t filesize = req.statbuf.st_size;
    uv_fs_req_cleanup(&req);

    std::vector<char> vec(filesize);
    size_t read = fread(vec.data(), filesize, 1, fp);
    assert(read == 1);
    assert(snapshot);
    int32_t ret = fclose(fp);
    assert(ret == 0);

    CHECK(node_api_env_options_use_snapshot(options, vec.data(), vec.size()));
  }

  if (is_building_snapshot) {
    // It contains at least the binary path, the code to snapshot,
    // and --embedder-snapshot-create (which is filtered, so at least
    // 2 arguments should remain after filtering).
    assert(filtered_args.size() >= 2);
    // Insert an anonymous filename as process.argv[1].
    filtered_args.insert(filtered_args.begin() + 1, "__node_anonymous_main");
  }

  CStringArray filtered_args_arr(filtered_args);
  CHECK(node_api_env_options_set_args(
      options, filtered_args_arr.size(), filtered_args_arr.cstrings()));

  if (!snapshot_blob_path.empty() && is_building_snapshot) {
    CHECK(node_api_env_options_create_snapshot(
        options,
        [](void* cb_data, const uint8_t* blob, size_t size) {
          const char* snapshot_blob_path = static_cast<const char*>(cb_data);
          FILE* fp = fopen(snapshot_blob_path, "wb");
          assert(fp != nullptr);

          size_t written = fwrite(blob, size, 1, fp);
          assert(written == 1);

          int32_t ret = fclose(fp);
          assert(ret == 0);
        },
        const_cast<char*>(snapshot_blob_path.c_str()),
        snapshot_flags));
  }

  napi_env env;
  if (use_snapshot) {
    CHECK(node_api_create_env(
        options, get_errors, nullptr, nullptr, NAPI_VERSION, &env));
  } else if (is_building_snapshot) {
    // Environment created for snapshotting must set process.argv[1] to
    // the name of the main script, which was inserted above.
    CHECK(node_api_create_env(
        options,
        get_errors,
        nullptr,
        "const assert = require('assert');"
        "assert(require('v8').startupSnapshot.isBuildingSnapshot());"
        "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };"
        "globalThis.require = require;"
        "require('vm').runInThisContext(process.argv[2]);",
        NAPI_VERSION,
        &env));
  } else {
    CHECK(node_api_create_env(
        options,
        get_errors,
        nullptr,
        "const publicRequire = require('module').createRequire(process.cwd() + "
        "'/');"
        "globalThis.require = publicRequire;"
        "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };"
        "require('vm').runInThisContext(process.argv[1]);",
        NAPI_VERSION,
        &env));
  }

  CHECK(node_api_delete_env(env, &exit_code));

  return exit_code;
}
