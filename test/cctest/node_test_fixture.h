#ifndef TEST_CCTEST_NODE_TEST_FIXTURE_H_
#define TEST_CCTEST_NODE_TEST_FIXTURE_H_

#include <stdlib.h>
#include "gtest/gtest.h"
#include "node.h"
#include "env.h"
#include "v8.h"
#include "libplatform/libplatform.h"

using node::Environment;
using node::IsolateData;
using node::CreateIsolateData;
using node::CreateEnvironment;
using node::AtExit;
using node::RunAtExit;

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    return AllocateUninitialized(length);
  }

  virtual void* AllocateUninitialized(size_t length) {
    return calloc(length, sizeof(int));
  }

  virtual void Free(void* data, size_t) {
    free(data);
  }
};

struct Argv {
 public:
  Argv(const char* prog, const char* arg1, const char* arg2) {
    int prog_len = strlen(prog) + 1;
    int arg1_len = strlen(arg1) + 1;
    int arg2_len = strlen(arg2) + 1;
    argv_ = static_cast<char**>(malloc(3 * sizeof(char*)));
    argv_[0] = static_cast<char*>(malloc(prog_len + arg1_len + arg2_len));
    snprintf(argv_[0], prog_len, "%s", prog);
    snprintf(argv_[0] + prog_len, arg1_len, "%s", arg1);
    snprintf(argv_[0] + prog_len + arg1_len, arg2_len, "%s", arg2);
    argv_[1] = argv_[0] + prog_len + 1;
    argv_[2] = argv_[0] + prog_len + arg1_len + 1;
  }

  ~Argv() {
    free(argv_[0]);
    free(argv_);
  }

  char** operator *() const {
    return argv_;
  }

 private:
  char** argv_;
};

class NodeTestFixture : public ::testing::Test {
 protected:
  v8::Isolate::CreateParams params_;
  ArrayBufferAllocator allocator_;
  v8::Isolate* isolate_;

  virtual void SetUp() {
    platform_ = v8::platform::CreateDefaultPlatform();
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
    params_.array_buffer_allocator = &allocator_;
    isolate_ = v8::Isolate::New(params_);
  }

  virtual void TearDown() {
    v8::V8::ShutdownPlatform();
    delete platform_;
    platform_ = nullptr;
  }

 private:
  v8::Platform* platform_;
};

#endif  // TEST_CCTEST_NODE_TEST_FIXTURE_H_
