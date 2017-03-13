#ifndef TEST_CCTEST_NODE_TEST_FIXTURE_H_
#define TEST_CCTEST_NODE_TEST_FIXTURE_H_

#include <stdlib.h>
#include "gtest/gtest.h"
#include "node.h"
#include "env.h"
#include "v8.h"
#include "libplatform/libplatform.h"

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    return AllocateUninitialized(length);
  }

  virtual void* AllocateUninitialized(size_t length) {
    return calloc(length, 1);
  }

  virtual void Free(void* data, size_t) {
    free(data);
  }
};

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

uv_loop_t current_loop;

class NodeTestFixture : public ::testing::Test {
 public:
  static uv_loop_t* CurrentLoop() { return &current_loop; }

 protected:
  v8::Isolate::CreateParams params_;
  ArrayBufferAllocator allocator_;
  v8::Isolate* isolate_;

  ~NodeTestFixture() {
    TearDown();
  }

  virtual void SetUp() {
    CHECK_EQ(0, uv_loop_init(&current_loop));
    platform_ = new node::NodePlatform(8, &current_loop, nullptr);
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
    params_.array_buffer_allocator = &allocator_;
    isolate_ = v8::Isolate::New(params_);
  }

  virtual void TearDown() {
    if (platform_ == nullptr) return;
    platform_->Shutdown();
    while (uv_loop_alive(&current_loop)) {
      uv_run(&current_loop, UV_RUN_ONCE);
    }
    v8::V8::ShutdownPlatform();
    delete platform_;
    platform_ = nullptr;
    CHECK_EQ(0, uv_loop_close(&current_loop));
  }

 private:
  node::NodePlatform* platform_ = nullptr;
};

#endif  // TEST_CCTEST_NODE_TEST_FIXTURE_H_
