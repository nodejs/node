#include "embedtest_node_api.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>

class CStringArray {
  static constexpr size_t kInplaceBufferSize = 32;

 public:
  explicit CStringArray(const std::vector<std::string>& strings) noexcept
      : size_(strings.size()) {
    if (size_ <= inplace_buffer_.size()) {
      c_strs_ = inplace_buffer_.data();
    } else {
      allocated_buffer_ = std::make_unique<const char*[]>(size_);
      c_strs_ = allocated_buffer_.get();
    }
    for (size_t i = 0; i < size_; ++i) {
      c_strs_[i] = strings[i].c_str();
    }
  }

  CStringArray(const CStringArray&) = delete;
  CStringArray& operator=(const CStringArray&) = delete;

  const char** c_strs() const { return c_strs_; }
  size_t size() const { return size_; }

 private:
  const char** c_strs_{};
  size_t size_{};
  std::array<const char*, kInplaceBufferSize> inplace_buffer_;
  std::unique_ptr<const char*[]> allocated_buffer_;
};

static int32_t RunNodeInstance(node_embedding_platform platform);

extern "C" int32_t test_main_snapshot_node_api(int32_t argc, char* argv[]) {
  CHECK(node_embedding_on_error(HandleTestError, argv[0]));

  node_embedding_platform platform;
  CHECK(node_embedding_create_platform(NODE_EMBEDDING_VERSION, &platform));
  CHECK(node_embedding_platform_set_args(platform, argc, argv));
  CHECK(node_embedding_platform_set_flags(
      platform, node_embedding_platform_disable_node_options_env));
  bool early_return = false;
  CHECK(node_embedding_platform_initialize(platform, &early_return));
  if (early_return) {
    return 0;
  }

  CHECK_EXIT_CODE(RunNodeInstance(platform));

  CHECK(node_embedding_delete_platform(platform));
  return 0;
}

int32_t RunNodeInstance(node_embedding_platform platform) {
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

  std::vector<std::string> args;
  CHECK(node_embedding_platform_get_args(platform, GetArgsVector, &args));

  std::vector<std::string> filtered_args;
  bool is_building_snapshot = false;
  node_embedding_snapshot_flags snapshot_flags =
      node_embedding_snapshot_no_flags;
  std::string snapshot_blob_path;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--embedder-snapshot-create") {
      is_building_snapshot = true;
    } else if (arg == "--embedder-snapshot-as-file") {
      // This parameter is not implemented by the Node-API, and we must not
      // include it in the filtered_args.
    } else if (arg == "--without-code-cache") {
      snapshot_flags = snapshot_flags | node_embedding_snapshot_no_code_cache;
    } else if (arg == "--embedder-snapshot-blob") {
      assert(i + 1 < args.size());
      snapshot_blob_path = args[i + 1];
      i++;
    } else {
      filtered_args.push_back(arg);
    }
  }

  node_embedding_runtime runtime;
  CHECK(node_embedding_create_runtime(platform, &runtime));

  bool use_snapshot = false;
  if (!snapshot_blob_path.empty() && !is_building_snapshot) {
    use_snapshot = true;
    FILE* fp = fopen(snapshot_blob_path.c_str(), "rb");
    assert(fp != nullptr);
    // Node-API only supports loading snapshots from blobs.
    fseek(fp, 0, SEEK_END);
    size_t filesize = static_cast<size_t>(ftell(fp));
    fseek(fp, 0, SEEK_SET);

    std::vector<uint8_t> vec(filesize);
    size_t read = fread(vec.data(), filesize, 1, fp);
    assert(read == 1);
    assert(snapshot);
    int32_t ret = fclose(fp);
    assert(ret == 0);

    CHECK(node_embedding_runtime_use_snapshot(runtime, vec.data(), vec.size()));
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
  CHECK(node_embedding_runtime_set_args(
      runtime, filtered_args_arr.size(), filtered_args_arr.c_strs()));

  if (!snapshot_blob_path.empty() && is_building_snapshot) {
    CHECK(node_embedding_runtime_on_create_snapshot(
        runtime,
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

  if (use_snapshot) {
    CHECK(node_embedding_runtime_initialize(runtime, nullptr));
  } else if (is_building_snapshot) {
    // Environment created for snapshotting must set process.argv[1] to
    // the name of the main script, which was inserted above.
    CHECK(node_embedding_runtime_initialize(
        runtime,
        "const assert = require('assert');"
        "assert(require('v8').startupSnapshot.isBuildingSnapshot());"
        "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };"
        "globalThis.require = require;"
        "require('vm').runInThisContext(process.argv[2]);"));
  } else {
    CHECK(node_embedding_runtime_initialize(runtime, main_script));
  }

  CHECK(node_embedding_delete_runtime(runtime));

  return 0;
}
