#ifndef TEST_CCTEST_NODE_TEST_FIXTURE_H_
#define TEST_CCTEST_NODE_TEST_FIXTURE_H_

#include <stdlib.h>
#include "gtest/gtest.h"
#include "node.h"
#include "node_platform.h"
#include "node_internals.h"
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

extern uv_loop_t current_loop;

class NodeTestFixture : public ::testing::Test {
 public:
  static uv_loop_t* CurrentLoop() { return &current_loop; }

  node::MultiIsolatePlatform* Platform() const { return platform_; }

 protected:
  v8::Isolate* isolate_;

  ~NodeTestFixture() {
    TearDown();
  }

  virtual void SetUp() {
    CHECK_EQ(0, uv_loop_init(&current_loop));
    platform_ = new node::NodePlatform(8, nullptr);
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
    v8::Isolate::CreateParams params_;
    params_.array_buffer_allocator = allocator_.get();
    isolate_ = v8::Isolate::New(params_);

    // As the TracingController is stored globally, we only need to create it
    // one time for all tests.
    if (node::tracing::TraceEventHelper::GetTracingController() == nullptr) {
      node::tracing::TraceEventHelper::SetTracingController(
          new v8::TracingController());
    }
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
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_{
      v8::ArrayBuffer::Allocator::NewDefaultAllocator()};
};


class EnvironmentTestFixture : public NodeTestFixture {
 public:
  class Env {
   public:
    Env(const v8::HandleScope& handle_scope,
        const Argv& argv,
        NodeTestFixture* test_fixture) {
      auto isolate = handle_scope.GetIsolate();
      context_ = node::NewContext(isolate);
      CHECK(!context_.IsEmpty());
      context_->Enter();

      isolate_data_ = node::CreateIsolateData(isolate,
                                              NodeTestFixture::CurrentLoop(),
                                              test_fixture->Platform());
      CHECK_NE(nullptr, isolate_data_);
      environment_ = node::CreateEnvironment(isolate_data_,
                                             context_,
                                             1, *argv,
                                             argv.nr_args(), *argv);
      CHECK_NE(nullptr, environment_);
    }

    ~Env() {
      environment_->CleanupHandles();
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

   private:
    v8::Local<v8::Context> context_;
    node::IsolateData* isolate_data_;
    node::Environment* environment_;
    DISALLOW_COPY_AND_ASSIGN(Env);
  };
};

#endif  // TEST_CCTEST_NODE_TEST_FIXTURE_H_
