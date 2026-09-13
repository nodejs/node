#include "node_diagnostics_channel.h"

#include "base_object-inl.h"
#include "gtest/gtest.h"
#include "node_test_fixture.h"

using node::BaseObjectPtr;
using node::diagnostics_channel::Channel;

class DiagnosticsChannelTest : public EnvironmentTestFixture {};

static v8::Local<v8::Value> RunJS(v8::Isolate* isolate, const char* code) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(
          context, v8::String::NewFromUtf8(isolate, code).ToLocalChecked())
          .ToLocalChecked();
  return script->Run(context).ToLocalChecked();
}

// Channel::HasSubscribers() returns false when there are no subscribers.
TEST_F(DiagnosticsChannelTest, HasSubscribersReturnsFalseWithoutSubscribers) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv};

  SetProcessExitHandler(*env, [&](node::Environment* env_, int exit_code) {
    EXPECT_EQ(exit_code, 0);
    node::Stop(*env);
  });

  // Load the environment to initialize bindings.
  node::LoadEnvironment(*env, "require('diagnostics_channel');");

  auto ch = Channel::Get(*env, "test:cctest:no-subscribers");
  ASSERT_NE(ch, nullptr);
  EXPECT_FALSE(ch->HasSubscribers());
}

// Channel::HasSubscribers() returns true after JS subscribes.
TEST_F(DiagnosticsChannelTest, HasSubscribersReturnsTrueAfterSubscribe) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv};

  SetProcessExitHandler(*env, [&](node::Environment* env_, int exit_code) {
    EXPECT_EQ(exit_code, 0);
    node::Stop(*env);
  });

  node::LoadEnvironment(*env,
                        "const dc = require('diagnostics_channel');"
                        "dc.subscribe('test:cctest:with-sub', () => {});");

  auto ch = Channel::Get(*env, "test:cctest:with-sub");
  ASSERT_NE(ch, nullptr);
  EXPECT_TRUE(ch->HasSubscribers());
}

// Channel::Get() with the same name returns consistent subscriber state.
TEST_F(DiagnosticsChannelTest, GetReturnsSameChannelState) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv};

  SetProcessExitHandler(*env, [&](node::Environment* env_, int exit_code) {
    EXPECT_EQ(exit_code, 0);
    node::Stop(*env);
  });

  node::LoadEnvironment(*env,
                        "const dc = require('diagnostics_channel');"
                        "dc.subscribe('test:cctest:same-channel', () => {});");

  auto ch1 = Channel::Get(*env, "test:cctest:same-channel");
  auto ch2 = Channel::Get(*env, "test:cctest:same-channel");
  ASSERT_NE(ch1, nullptr);
  ASSERT_NE(ch2, nullptr);
  EXPECT_TRUE(ch1->HasSubscribers());
  EXPECT_TRUE(ch2->HasSubscribers());
  EXPECT_EQ(ch1, ch2);
}

// Channel::Publish() delivers messages to JS subscribers.
TEST_F(DiagnosticsChannelTest, PublishDeliversToJSSubscribers) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv};

  SetProcessExitHandler(*env, [&](node::Environment* env_, int exit_code) {
    EXPECT_EQ(exit_code, 0);
    node::Stop(*env);
  });

  node::LoadEnvironment(
      *env,
      "const dc = require('diagnostics_channel');"
      "const assert = require('assert');"
      "dc.subscribe('test:cctest:publish', (message, name) => {"
      "  assert.strictEqual(name, 'test:cctest:publish');"
      "  assert.strictEqual(message.value, 42);"
      "  globalThis.__publishReceived = true;"
      "});");

  v8::Local<v8::Context> context = (*env)->context();

  auto ch = Channel::Get(*env, "test:cctest:publish");
  ASSERT_NE(ch, nullptr);
  ASSERT_TRUE(ch->HasSubscribers());

  v8::Local<v8::Object> msg = v8::Object::New(isolate_);
  msg->Set(context,
           v8::String::NewFromUtf8Literal(isolate_, "value"),
           v8::Integer::New(isolate_, 42))
      .Check();

  ch->Publish(*env, msg);

  v8::Local<v8::Value> received =
      context->Global()
          ->Get(context,
                v8::String::NewFromUtf8Literal(isolate_, "__publishReceived"))
          .ToLocalChecked();
  EXPECT_TRUE(received->IsTrue());
}

// C++ creates a channel first, then JS subscribes to the same name.
// Verifies C++ Channel reflects the JS subscriber via the shared buffer.
TEST_F(DiagnosticsChannelTest, CppChannelVisibleFromJS) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv};

  SetProcessExitHandler(*env, [&](node::Environment* env_, int exit_code) {
    EXPECT_EQ(exit_code, 0);
    node::Stop(*env);
  });

  // Expose dc on globalThis so RunJS (v8::Script) can access it.
  node::LoadEnvironment(*env,
                        "globalThis.__dc = require('diagnostics_channel');");

  auto ch = Channel::Get(*env, "test:cctest:cpp-first");
  ASSERT_NE(ch, nullptr);
  EXPECT_FALSE(ch->HasSubscribers());

  // JS subscribes to the same channel name via globalThis.__dc.
  RunJS(isolate_,
        "globalThis.__dc.subscribe('test:cctest:cpp-first', () => {});");

  EXPECT_TRUE(ch->HasSubscribers());

  RunJS(isolate_,
        "globalThis.__cppFirstMsg = null;"
        "globalThis.__dc.subscribe('test:cctest:cpp-first', (msg) => {"
        "  globalThis.__cppFirstMsg = msg;"
        "});");

  v8::Local<v8::Context> context = (*env)->context();
  v8::Local<v8::Object> msg = v8::Object::New(isolate_);
  msg->Set(context,
           v8::String::NewFromUtf8Literal(isolate_, "from"),
           v8::String::NewFromUtf8Literal(isolate_, "cpp"))
      .Check();

  ch->Publish(*env, msg);

  v8::Local<v8::Value> received =
      context->Global()
          ->Get(context,
                v8::String::NewFromUtf8Literal(isolate_, "__cppFirstMsg"))
          .ToLocalChecked();
  ASSERT_TRUE(received->IsObject());
  v8::Local<v8::Value> from_val =
      received.As<v8::Object>()
          ->Get(context, v8::String::NewFromUtf8Literal(isolate_, "from"))
          .ToLocalChecked();
  v8::String::Utf8Value from_str(isolate_, from_val);
  EXPECT_STREQ(*from_str, "cpp");
}

// JS creates a channel and subscribes, then C++ gets the same channel,
// verifies it shares state, and publishes messages that JS receives.
TEST_F(DiagnosticsChannelTest, JSChannelVisibleFromCpp) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv};

  SetProcessExitHandler(*env, [&](node::Environment* env_, int exit_code) {
    EXPECT_EQ(exit_code, 0);
    node::Stop(*env);
  });

  node::LoadEnvironment(*env,
                        "const dc = require('diagnostics_channel');"
                        "globalThis.__dc = dc;"
                        "globalThis.__jsFirstMessages = [];"
                        "dc.subscribe('test:cctest:js-first', (msg) => {"
                        "  globalThis.__jsFirstMessages.push(msg);"
                        "});");

  v8::Local<v8::Context> context = (*env)->context();

  auto ch = Channel::Get(*env, "test:cctest:js-first");
  ASSERT_NE(ch, nullptr);
  ASSERT_TRUE(ch->HasSubscribers());

  // Publish from C++ — JS subscriber should receive it.
  v8::Local<v8::Object> msg1 = v8::Object::New(isolate_);
  msg1->Set(context,
            v8::String::NewFromUtf8Literal(isolate_, "seq"),
            v8::Integer::New(isolate_, 1))
      .Check();
  ch->Publish(*env, msg1);

  v8::Local<v8::Object> msg2 = v8::Object::New(isolate_);
  msg2->Set(context,
            v8::String::NewFromUtf8Literal(isolate_, "seq"),
            v8::Integer::New(isolate_, 2))
      .Check();
  ch->Publish(*env, msg2);

  v8::Local<v8::Value> msgs_val =
      context->Global()
          ->Get(context,
                v8::String::NewFromUtf8Literal(isolate_, "__jsFirstMessages"))
          .ToLocalChecked();
  ASSERT_TRUE(msgs_val->IsArray());
  v8::Local<v8::Array> msgs = msgs_val.As<v8::Array>();
  EXPECT_EQ(msgs->Length(), 2u);

  // Check first message: { seq: 1 }
  v8::Local<v8::Value> m1 = msgs->Get(context, 0).ToLocalChecked();
  ASSERT_TRUE(m1->IsObject());
  v8::Local<v8::Value> seq1 =
      m1.As<v8::Object>()
          ->Get(context, v8::String::NewFromUtf8Literal(isolate_, "seq"))
          .ToLocalChecked();
  EXPECT_EQ(seq1->Int32Value(context).FromJust(), 1);

  // Check second message: { seq: 2 }
  v8::Local<v8::Value> m2 = msgs->Get(context, 1).ToLocalChecked();
  ASSERT_TRUE(m2->IsObject());
  v8::Local<v8::Value> seq2 =
      m2.As<v8::Object>()
          ->Get(context, v8::String::NewFromUtf8Literal(isolate_, "seq"))
          .ToLocalChecked();
  EXPECT_EQ(seq2->Int32Value(context).FromJust(), 2);

  RunJS(isolate_,
        "globalThis.__jsHasSubs ="
        "  globalThis.__dc.hasSubscribers('test:cctest:js-first');");

  v8::Local<v8::Value> js_has_subs =
      context->Global()
          ->Get(context,
                v8::String::NewFromUtf8Literal(isolate_, "__jsHasSubs"))
          .ToLocalChecked();
  EXPECT_TRUE(js_has_subs->IsTrue());
  EXPECT_TRUE(ch->HasSubscribers());
}

// Native channels grow the shared subscriber storage past its initial
// capacity without losing the state of channels that were already linked.
// Updating the first and last channels after growth also verifies that JS uses
// the replacement buffer instead of a stale cached reference.
TEST_F(DiagnosticsChannelTest, NativeChannelsGrowSubscriberStorage) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;
  Env env{handle_scope, argv};

  SetProcessExitHandler(*env, [&](node::Environment* env_, int exit_code) {
    EXPECT_EQ(exit_code, 0);
    node::Stop(*env);
  });

  node::LoadEnvironment(
      *env,
      "globalThis.__dc = require('diagnostics_channel');"
      "globalThis.__firstSubscriber = () => {};"
      "globalThis.__dc.subscribe('test:cctest:grow:0', "
      "                          globalThis.__firstSubscriber);");

  auto first = Channel::Get(*env, "test:cctest:grow:0");
  ASSERT_TRUE(first);
  ASSERT_TRUE(first->HasSubscribers());

  BaseObjectPtr<Channel> last;
  for (size_t i = 1; i <= 1024; i++) {
    std::string name = "test:cctest:grow:" + std::to_string(i);
    last = Channel::Get(*env, name.c_str());
    ASSERT_TRUE(last);
  }

  RunJS(isolate_,
        "globalThis.__dc.unsubscribe('test:cctest:grow:0', "
        "                            globalThis.__firstSubscriber);");
  EXPECT_FALSE(first->HasSubscribers());

  RunJS(isolate_,
        "globalThis.__lastSubscriber = () => {};"
        "globalThis.__dc.subscribe('test:cctest:grow:1024', "
        "                          globalThis.__lastSubscriber);");
  EXPECT_TRUE(last->HasSubscribers());
}

// Mirrors how node:sqlite holds a Channel: a strong reference kept for the
// lifetime of a BaseObject, which is destroyed during environment cleanup
// after the BindingData that owns the channel is already gone.
class ChannelHolder : public node::BaseObject {
 public:
  ChannelHolder(node::Environment* env,
                v8::Local<v8::Object> obj,
                BaseObjectPtr<Channel> channel)
      : BaseObject(env, obj), channel_(std::move(channel)) {}

  ~ChannelHolder() override {
    destroyed = true;
    // Would read the destroyed BindingData if the back-pointer were stale.
    had_subscribers = channel_->HasSubscribers();
  }

  static bool destroyed;
  static bool had_subscribers;

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(ChannelHolder)
  SET_SELF_SIZE(ChannelHolder)

 private:
  BaseObjectPtr<Channel> channel_;
};

bool ChannelHolder::destroyed = false;
bool ChannelHolder::had_subscribers = false;

// A Channel whose holder outlives the BindingData must report no subscribers
// rather than reading the destroyed binding's subscriber array.
TEST_F(DiagnosticsChannelTest, ChannelOutlivingBindingHasNoSubscribers) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;

  ChannelHolder::destroyed = false;
  ChannelHolder::had_subscribers = false;

  {
    Env env{handle_scope, argv};

    SetProcessExitHandler(*env, [&](node::Environment* env_, int exit_code) {
      EXPECT_EQ(exit_code, 0);
      node::Stop(*env);
    });

    node::LoadEnvironment(
        *env,
        "const dc = require('diagnostics_channel');"
        "dc.subscribe('test:cctest:outlives-binding', () => {});");

    auto channel = Channel::Get(*env, "test:cctest:outlives-binding");
    ASSERT_TRUE(channel);
    ASSERT_TRUE(channel->HasSubscribers());

    v8::Local<v8::Context> context = (*env)->context();
    v8::Local<v8::Object> obj =
        node::BaseObject::MakeLazilyInitializedJSTemplate(*env)
            ->GetFunction(context)
            .ToLocalChecked()
            ->NewInstance(context)
            .ToLocalChecked();
    // MakeBaseObject leaves the holder a strong root, so it survives until the
    // environment is torn down, like a DatabaseSync still reachable at exit.
    node::MakeBaseObject<ChannelHolder>(*env, obj, channel);
  }

  EXPECT_TRUE(ChannelHolder::destroyed);
  EXPECT_FALSE(ChannelHolder::had_subscribers);
}
