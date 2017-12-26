#ifndef TEST_CCTEST_NODE_TEST_FIXTURE_H_
#define TEST_CCTEST_NODE_TEST_FIXTURE_H_

#include <stdlib.h>
#include "gtest/gtest.h"
#include "node.h"
#include "node_internals.h"
#include "env.h"
#include "v8.h"
#include "libplatform/libplatform.h"

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
  Argv() : Argv("node", "-p", "process.version") {}

  Argv(const char* prog, const char* arg1, const char* arg2) {
    int prog_len = strlen(prog) + 1;
    int arg1_len = strlen(arg1) + 1;
    int arg2_len = strlen(arg2) + 1;
    argv_ = static_cast<char**>(malloc(3 * sizeof(char*)));
    argv_[0] = static_cast<char*>(malloc(prog_len + arg1_len + arg2_len));
    snprintf(argv_[0], prog_len, "%s", prog);
    snprintf(argv_[0] + prog_len, arg1_len, "%s", arg1);
    snprintf(argv_[0] + prog_len + arg1_len, arg2_len, "%s", arg2);
    argv_[1] = argv_[0] + prog_len;
    argv_[2] = argv_[0] + prog_len + arg1_len;
    nr_args_ = 3;
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
 protected:
  static v8::Isolate::CreateParams params_;
  static ArrayBufferAllocator allocator_;
  static v8::Platform* platform_;
  static uv_loop_t current_loop;
  v8::Isolate* isolate_;

  ~NodeTestFixture() {
    TearDown();
  }

  static void SetUpTestCase() {
    platform_ = v8::platform::CreateDefaultPlatform();
    CHECK_EQ(0, uv_loop_init(&current_loop));
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
    params_.array_buffer_allocator = &allocator_;
  }

  static void TearDownTestCase() {
    if (platform_ == nullptr) return;
    while (uv_loop_alive(&current_loop)) {
      uv_run(&current_loop, UV_RUN_ONCE);
    }
    v8::V8::ShutdownPlatform();
    delete platform_;
    platform_ = nullptr;

    uv_loop_close(&current_loop);
  }

  virtual void SetUp() {
    isolate_ = v8::Isolate::New(params_);
  }
};


class EnvironmentTestFixture : public NodeTestFixture {
 public:
  class Env {
   public:
    Env(const v8::HandleScope& handle_scope,
        const Argv& argv) {
      auto isolate = handle_scope.GetIsolate();
      context_ = v8::Context::New(isolate);
      CHECK(!context_.IsEmpty());
      context_->Enter();

      v8::Context::Scope context_scope(context_);

      environment_ = node::CreateEnvironment(isolate,
                                             &NodeTestFixture::current_loop,
                                             context_,
                                             1,
                                             *argv,
                                             argv.nr_args(),
                                             *argv);

      CHECK_NE(nullptr, environment_);
    }

    ~Env() {
      node::FreeEnvironment(environment_);
      context_->Exit();
    }

    node::Environment* operator*() const {
      return environment_;
    }

    v8::Local<v8::Context> context()  const {
      return context_;
    }

   private:
    v8::Local<v8::Context> context_;
    node::Environment* environment_;
    DISALLOW_COPY_AND_ASSIGN(Env);
  };
};

#endif  // TEST_CCTEST_NODE_TEST_FIXTURE_H_
