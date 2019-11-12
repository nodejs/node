#ifndef TEST_CCTEST_NODE_TEST_FIXTURE_H_
#define TEST_CCTEST_NODE_TEST_FIXTURE_H_

#include <cstdlib>
#include <memory>
#include "gtest/gtest.h"
#include "node.h"
#include "node_platform.h"
#include "node_internals.h"
#include "env-inl.h"
#include "util-inl.h"
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

using ArrayBufferUniquePtr = std::unique_ptr<node::ArrayBufferAllocator,
      decltype(&node::FreeArrayBufferAllocator)>;
using TracingAgentUniquePtr = std::unique_ptr<node::tracing::Agent>;
using NodePlatformUniquePtr = std::unique_ptr<node::NodePlatform>;

class NodeZeroIsolateTestFixture : public ::testing::Test {
 protected:
  static ArrayBufferUniquePtr allocator;
  static TracingAgentUniquePtr tracing_agent;
  static NodePlatformUniquePtr platform;
  static uv_loop_t current_loop;
  static bool node_initialized;

  static void SetUpTestCase() {
    if (!node_initialized) {
      node_initialized = true;
      int argc = 1;
      const char* argv0 = "cctest";
      int exec_argc;
      const char** exec_argv;
      node::Init(&argc, &argv0, &exec_argc, &exec_argv);
    }

    tracing_agent = std::make_unique<node::tracing::Agent>();
    node::tracing::TraceEventHelper::SetAgent(tracing_agent.get());
    CHECK_EQ(0, uv_loop_init(&current_loop));
    platform.reset(static_cast<node::NodePlatform*>(
          node::InitializeV8Platform(4)));
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

  void SetUp() override {
    allocator = ArrayBufferUniquePtr(node::CreateArrayBufferAllocator(),
                                     &node::FreeArrayBufferAllocator);
  }
};


class NodeTestFixture : public NodeZeroIsolateTestFixture {
 protected:
  v8::Isolate* isolate_;

  void SetUp() override {
    NodeZeroIsolateTestFixture::SetUp();
    isolate_ = NewIsolate(allocator.get(), &current_loop);
    CHECK_NOT_NULL(isolate_);
    isolate_->Enter();
  }

  void TearDown() override {
    platform->DrainTasks(isolate_);
    isolate_->Exit();
    isolate_->Dispose();
    platform->UnregisterIsolate(isolate_);
    isolate_ = nullptr;
    NodeZeroIsolateTestFixture::TearDown();
  }
};


class EnvironmentTestFixture : public NodeTestFixture {
 public:
  class Env {
   public:
    Env(const v8::HandleScope& handle_scope, const Argv& argv) {
      auto isolate = handle_scope.GetIsolate();
      context_ = node::NewContext(isolate);
      CHECK(!context_.IsEmpty());
      context_->Enter();

      isolate_data_ = node::CreateIsolateData(isolate,
                                              &NodeTestFixture::current_loop,
                                              platform.get());
      CHECK_NE(nullptr, isolate_data_);
      environment_ = node::CreateEnvironment(isolate_data_,
                                             context_,
                                             1, *argv,
                                             argv.nr_args(), *argv);
      CHECK_NE(nullptr, environment_);
    }

    ~Env() {
      node::FreeEnvironment(environment_);
      node::FreeIsolateData(isolate_data_);
      context_->Exit();
    }

    node::Environment* operator*() const {
      return environment_;
    }

    v8::Local<v8::Context> context()  const {
      return context_;
    }

    Env(const Env&) = delete;
    Env& operator=(const Env&) = delete;

   private:
    v8::Local<v8::Context> context_;
    node::IsolateData* isolate_data_;
    node::Environment* environment_;
  };
};

#endif  // TEST_CCTEST_NODE_TEST_FIXTURE_H_
