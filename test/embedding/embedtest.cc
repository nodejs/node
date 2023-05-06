#ifdef NDEBUG
#undef NDEBUG
#endif
#include "node.h"
#include "uv.h"
#include <assert.h>

// Note: This file is being referred to from doc/api/embedding.md, and excerpts
// from it are included in the documentation. Try to keep these in sync.
// Snapshot support is not part of the embedder API docs yet due to its
// experimental nature, although it is of course documented in node.h.

using node::CommonEnvironmentSetup;
using node::Environment;
using node::MultiIsolatePlatform;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Locker;
using v8::MaybeLocal;
using v8::V8;
using v8::Value;

static int RunNodeInstance(MultiIsolatePlatform* platform,
                           const std::vector<std::string>& args,
                           const std::vector<std::string>& exec_args);

int main(int argc, char** argv) {
  argv = uv_setup_args(argc, argv);
  std::vector<std::string> args(argv, argv + argc);
  std::unique_ptr<node::InitializationResult> result =
      node::InitializeOncePerProcess(
          args,
          {node::ProcessInitializationFlags::kNoInitializeV8,
           node::ProcessInitializationFlags::kNoInitializeNodeV8Platform});

  for (const std::string& error : result->errors())
    fprintf(stderr, "%s: %s\n", args[0].c_str(), error.c_str());
  if (result->early_return() != 0) {
    return result->exit_code();
  }

  std::unique_ptr<MultiIsolatePlatform> platform =
      MultiIsolatePlatform::Create(4);
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  int ret =
      RunNodeInstance(platform.get(), result->args(), result->exec_args());

  V8::Dispose();
  V8::DisposePlatform();

  node::TearDownOncePerProcess();
  return ret;
}

int RunNodeInstance(MultiIsolatePlatform* platform,
                    const std::vector<std::string>& args,
                    const std::vector<std::string>& exec_args) {
  int exit_code = 0;

  node::EmbedderSnapshotData::Pointer snapshot;
  auto snapshot_build_mode_it =
      std::find(args.begin(), args.end(), "--embedder-snapshot-create");
  auto snapshot_arg_it =
      std::find(args.begin(), args.end(), "--embedder-snapshot-blob");
  auto snapshot_as_file_it =
      std::find(args.begin(), args.end(), "--embedder-snapshot-as-file");
  if (snapshot_arg_it < args.end() - 1 &&
      snapshot_build_mode_it == args.end()) {
    const char* filename = (snapshot_arg_it + 1)->c_str();
    FILE* fp = fopen(filename, "r");
    assert(fp != nullptr);
    if (snapshot_as_file_it != args.end()) {
      snapshot = node::EmbedderSnapshotData::FromFile(fp);
    } else {
      uv_fs_t req = uv_fs_t();
      int statret = uv_fs_stat(nullptr, &req, filename, nullptr);
      assert(statret == 0);
      size_t filesize = req.statbuf.st_size;
      uv_fs_req_cleanup(&req);

      std::vector<char> vec(filesize);
      size_t read = fread(vec.data(), filesize, 1, fp);
      assert(read == 1);
      snapshot = node::EmbedderSnapshotData::FromBlob(vec);
    }
    assert(snapshot);
    int ret = fclose(fp);
    assert(ret == 0);
  }

  std::vector<std::string> errors;
  std::unique_ptr<CommonEnvironmentSetup> setup =
      snapshot ? CommonEnvironmentSetup::CreateFromSnapshot(
                     platform, &errors, snapshot.get(), args, exec_args)
      : snapshot_build_mode_it != args.end()
          ? CommonEnvironmentSetup::CreateForSnapshotting(
                platform, &errors, args, exec_args)
          : CommonEnvironmentSetup::Create(platform, &errors, args, exec_args);
  if (!setup) {
    for (const std::string& err : errors)
      fprintf(stderr, "%s: %s\n", args[0].c_str(), err.c_str());
    return 1;
  }

  Isolate* isolate = setup->isolate();
  Environment* env = setup->env();

  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(setup->context());

    MaybeLocal<Value> loadenv_ret;
    if (snapshot) {
      loadenv_ret = node::LoadEnvironment(env, node::StartExecutionCallback{});
    } else {
      loadenv_ret = node::LoadEnvironment(
          env,
          // Snapshots do not support userland require()s (yet)
          "if (!require('v8').startupSnapshot.isBuildingSnapshot()) {"
          "  const publicRequire ="
          "    require('module').createRequire(process.cwd() + '/');"
          "  globalThis.require = publicRequire;"
          "} else globalThis.require = require;"
          "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };"
          "require('vm').runInThisContext(process.argv[1]);");
    }

    if (loadenv_ret.IsEmpty())  // There has been a JS exception.
      return 1;

    exit_code = node::SpinEventLoop(env).FromMaybe(1);
  }

  if (snapshot_arg_it < args.end() - 1 &&
      snapshot_build_mode_it != args.end()) {
    snapshot = setup->CreateSnapshot();
    assert(snapshot);

    FILE* fp = fopen((snapshot_arg_it + 1)->c_str(), "w");
    assert(fp != nullptr);
    if (snapshot_as_file_it != args.end()) {
      snapshot->ToFile(fp);
    } else {
      const std::vector<char> vec = snapshot->ToBlob();
      size_t written = fwrite(vec.data(), vec.size(), 1, fp);
      assert(written == 1);
    }
    int ret = fclose(fp);
    assert(ret == 0);
  }

  node::Stop(env);

  return exit_code;
}
