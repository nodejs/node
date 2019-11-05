#include "node_internals.h"
#include "libplatform/libplatform.h"

#include <string>
#include "gtest/gtest.h"
#include "node_test_fixture.h"

using node::AtExit;
using node::RunAtExit;

static bool called_cb_1 = false;
static bool called_cb_2 = false;
static bool called_cb_ordered_1 = false;
static bool called_cb_ordered_2 = false;
static bool called_at_exit_js = false;
static void at_exit_callback1(void* arg);
static void at_exit_callback2(void* arg);
static void at_exit_callback_ordered1(void* arg);
static void at_exit_callback_ordered2(void* arg);
static void at_exit_js(void* arg);
static std::string cb_1_arg;  // NOLINT(runtime/string)

class EnvironmentTest : public EnvironmentTestFixture {
 private:
  void TearDown() override {
    NodeTestFixture::TearDown();
    called_cb_1 = false;
    called_cb_2 = false;
    called_cb_ordered_1 = false;
    called_cb_ordered_2 = false;
  }
};

TEST_F(EnvironmentTest, PreExeuctionPreparation) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env {handle_scope, argv};

  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  const char* run_script = "process.argv0";
  v8::Local<v8::Script> script = v8::Script::Compile(
      context,
      v8::String::NewFromOneByte(isolate_,
                                 reinterpret_cast<const uint8_t*>(run_script),
                                 v8::NewStringType::kNormal).ToLocalChecked())
      .ToLocalChecked();
  v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
  CHECK(result->IsString());
}

TEST_F(EnvironmentTest, AtExitWithEnvironment) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env {handle_scope, argv};

  AtExit(*env, at_exit_callback1);
  RunAtExit(*env);
  EXPECT_TRUE(called_cb_1);
}

TEST_F(EnvironmentTest, AtExitWithoutEnvironment) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env {handle_scope, argv};

  AtExit(at_exit_callback1);  // No Environment is passed to AtExit.
  RunAtExit(*env);
  EXPECT_TRUE(called_cb_1);
}

TEST_F(EnvironmentTest, AtExitOrder) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env {handle_scope, argv};

  // Test that callbacks are run in reverse order.
  AtExit(*env, at_exit_callback_ordered1);
  AtExit(*env, at_exit_callback_ordered2);
  RunAtExit(*env);
  EXPECT_TRUE(called_cb_ordered_1);
  EXPECT_TRUE(called_cb_ordered_2);
}

TEST_F(EnvironmentTest, AtExitWithArgument) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env {handle_scope, argv};

  std::string arg{"some args"};
  AtExit(*env, at_exit_callback1, static_cast<void*>(&arg));
  RunAtExit(*env);
  EXPECT_EQ(arg, cb_1_arg);
}

TEST_F(EnvironmentTest, AtExitRunsJS) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env {handle_scope, argv};

  AtExit(*env, at_exit_js, static_cast<void*>(isolate_));
  EXPECT_FALSE(called_at_exit_js);
  RunAtExit(*env);
  EXPECT_TRUE(called_at_exit_js);
}

TEST_F(EnvironmentTest, MultipleEnvironmentsPerIsolate) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env1 {handle_scope, argv};
  Env env2 {handle_scope, argv};

  AtExit(*env1, at_exit_callback1);
  AtExit(*env2, at_exit_callback2);
  RunAtExit(*env1);
  EXPECT_TRUE(called_cb_1);
  EXPECT_FALSE(called_cb_2);

  RunAtExit(*env2);
  EXPECT_TRUE(called_cb_2);
}

TEST_F(EnvironmentTest, NoEnvironmentSanity) {
  const v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  EXPECT_EQ(node::Environment::GetCurrent(context), nullptr);
  EXPECT_EQ(node::GetCurrentEnvironment(context), nullptr);
  EXPECT_EQ(node::Environment::GetCurrent(isolate_), nullptr);

  v8::Context::Scope context_scope(context);
  EXPECT_EQ(node::Environment::GetCurrent(context), nullptr);
  EXPECT_EQ(node::GetCurrentEnvironment(context), nullptr);
  EXPECT_EQ(node::Environment::GetCurrent(isolate_), nullptr);
}

TEST_F(EnvironmentTest, NonNodeJSContext) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env {handle_scope, argv};

  EXPECT_EQ(node::Environment::GetCurrent(v8::Local<v8::Context>()), nullptr);

  node::Environment* env = *test_env;
  EXPECT_EQ(node::Environment::GetCurrent(isolate_), env);
  EXPECT_EQ(node::Environment::GetCurrent(env->context()), env);
  EXPECT_EQ(node::GetCurrentEnvironment(env->context()), env);

  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  EXPECT_EQ(node::Environment::GetCurrent(context), nullptr);
  EXPECT_EQ(node::GetCurrentEnvironment(context), nullptr);
  EXPECT_EQ(node::Environment::GetCurrent(isolate_), env);

  v8::Context::Scope context_scope(context);
  EXPECT_EQ(node::Environment::GetCurrent(context), nullptr);
  EXPECT_EQ(node::GetCurrentEnvironment(context), nullptr);
  EXPECT_EQ(node::Environment::GetCurrent(isolate_), nullptr);
}

static void at_exit_callback1(void* arg) {
  called_cb_1 = true;
  if (arg) {
    cb_1_arg = *static_cast<std::string*>(arg);
  }
}

static void at_exit_callback2(void* arg) {
  called_cb_2 = true;
}

static void at_exit_callback_ordered1(void* arg) {
  EXPECT_TRUE(called_cb_ordered_2);
  called_cb_ordered_1 = true;
}

static void at_exit_callback_ordered2(void* arg) {
  EXPECT_FALSE(called_cb_ordered_1);
  called_cb_ordered_2 = true;
}

static void at_exit_js(void* arg) {
  v8::Isolate* isolate = static_cast<v8::Isolate*>(arg);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> obj = v8::Object::New(isolate);
  assert(!obj.IsEmpty());  // Assert VM is still alive.
  assert(obj->IsObject());
  called_at_exit_js = true;
}
