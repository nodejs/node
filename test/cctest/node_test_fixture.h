#ifndef TEST_CCTEST_NODE_TEST_FIXTURE_H_
#define TEST_CCTEST_NODE_TEST_FIXTURE_H_

#include <stdlib.h>
#include "gtest/gtest.h"
#include "node.h"
#include "node_platform.h"
#include "env.h"
#include "v8.h"
#include "libplatform/libplatform.h"

struct Argv {
 public:
  Argv() : Argv({"node", "-p", "process.version"}) {}

  Argv(const std::initializer_list<const char*> &args) {
    nr_args_ = args.size();
    int total_len = 0;
    for (auto it = args.begin(); it != args.end(); ++it) {
      total_len += strlen(*it) + 1;
    }
    argv_ = static_cast<char**>(malloc(nr_args_ * sizeof(char*)));
    argv_[0] = static_cast<char*>(malloc(total_len));
    int i = 0;
    int offset = 0;
    for (auto it = args.begin(); it != args.end(); ++it, ++i) {
      int len = strlen(*it) + 1;
      snprintf(argv_[0] + offset, len, "%s", *it);
      // Skip argv_[0] as it points the correct location already
      if (i > 0) {
        argv_[i] = argv_[0] + offset;
      }
      offset += len;
    }
  }

  ~Argv() {
    free(argv_[0]);
    free(argv_);
  }

  int nr_args() const {
    return nr_args_;
  }

  char** operator*() const {
    return argv_;
  }

 private:
  char** argv_;
  int nr_args_;
};


class NodeTestFixture : public ::testing::Test {
 public:
  static uv_loop_t* CurrentLoop() { return &current_loop; }

  node::MultiIsolatePlatform* Platform() const { return platform.get(); }

 protected:
  static std::unique_ptr<v8::ArrayBuffer::Allocator> allocator;
  static std::unique_ptr<node::NodePlatform> platform;
  static v8::Isolate::CreateParams params;
  static uv_loop_t current_loop;
  v8::Isolate* isolate_;

  static void SetUpTestCase() {
    platform.reset(new node::NodePlatform(4, nullptr));
    allocator.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
    params.array_buffer_allocator = allocator.get();
    CHECK_EQ(0, uv_loop_init(&current_loop));
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
  }

  static void TearDownTestCase() {
    platform->Shutdown();
    while (uv_loop_alive(&current_loop)) {
      uv_run(&current_loop, UV_RUN_ONCE);
    }
    v8::V8::ShutdownPlatform();
    CHECK_EQ(0, uv_loop_close(&current_loop));
  }

  virtual void SetUp() {
    isolate_ = v8::Isolate::New(params);
    CHECK_NE(isolate_, nullptr);
  }

  virtual void TearDown() {
    isolate_->Dispose();
    isolate_ = nullptr;
  }
};

#endif  // TEST_CCTEST_NODE_TEST_FIXTURE_H_
