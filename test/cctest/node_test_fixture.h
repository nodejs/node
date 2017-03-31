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
  Argv() : Argv({"node", "-p", "process.version"}) {}

  Argv(const std::initializer_list<const char*> &args) {
    int nrArgs = args.size();
    int totalLen = 0;
    for (auto it = args.begin(); it != args.end(); ++it) {
      totalLen += strlen(*it) + 1;
    }
    argv_ = static_cast<char**>(malloc(nrArgs * sizeof(char*)));
    argv_[0] = static_cast<char*>(malloc(totalLen));
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
