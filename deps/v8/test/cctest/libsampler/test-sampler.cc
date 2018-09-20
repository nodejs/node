// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Tests of sampler functionalities.

#include "src/libsampler/sampler.h"

#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "test/cctest/cctest.h"


namespace v8 {
namespace sampler {

namespace {

class TestSamplingThread : public base::Thread {
 public:
  static const int kSamplerThreadStackSize = 64 * 1024;

  explicit TestSamplingThread(Sampler* sampler)
      : Thread(base::Thread::Options("TestSamplingThread",
                                     kSamplerThreadStackSize)),
        sampler_(sampler) {}

  // Implement Thread::Run().
  void Run() override {
    while (sampler_->IsProfiling()) {
      sampler_->DoSample();
      base::OS::Sleep(base::TimeDelta::FromMilliseconds(1));
    }
  }

 private:
  Sampler* sampler_;
};


class TestSampler : public Sampler {
 public:
  explicit TestSampler(Isolate* isolate) : Sampler(isolate) {}

  void SampleStack(const v8::RegisterState& regs) override {
    void* frames[kMaxFramesCount];
    SampleInfo sample_info;
    isolate()->GetStackSample(regs, frames, kMaxFramesCount, &sample_info);
    if (is_counting_samples_) {
      if (sample_info.vm_state == JS) ++js_sample_count_;
      if (sample_info.vm_state == EXTERNAL) ++external_sample_count_;
    }
  }
};


class TestApiCallbacks {
 public:
  TestApiCallbacks() {}

  static void Getter(v8::Local<v8::String> name,
                     const v8::PropertyCallbackInfo<v8::Value>& info) {
  }

  static void Setter(v8::Local<v8::String> name,
                     v8::Local<v8::Value> value,
                     const v8::PropertyCallbackInfo<void>& info) {
  }
};


static void RunSampler(v8::Local<v8::Context> env,
                       v8::Local<v8::Function> function,
                       v8::Local<v8::Value> argv[], int argc,
                       unsigned min_js_samples = 0,
                       unsigned min_external_samples = 0) {
  Sampler::SetUp();
  TestSampler* sampler = new TestSampler(env->GetIsolate());
  TestSamplingThread* thread = new TestSamplingThread(sampler);
  sampler->IncreaseProfilingDepth();
  sampler->Start();
  sampler->StartCountingSamples();
  thread->StartSynchronously();
  do {
    function->Call(env, env->Global(), argc, argv).ToLocalChecked();
  } while (sampler->js_sample_count() < min_js_samples ||
           sampler->external_sample_count() < min_external_samples);
  sampler->Stop();
  sampler->DecreaseProfilingDepth();
  thread->Join();
  delete thread;
  delete sampler;
  Sampler::TearDown();
}

}  // namespace

static const char* sampler_test_source = "function start(count) {\n"
"  for (var i = 0; i < count; i++) {\n"
"    var o = instance.foo;\n"
"    instance.foo = o + 1;\n"
"  }\n"
"}\n";

static v8::Local<v8::Function> GetFunction(v8::Local<v8::Context> env,
                                           const char* name) {
  return v8::Local<v8::Function>::Cast(
      env->Global()->Get(env, v8_str(name)).ToLocalChecked());
}


TEST(LibSamplerCollectSample) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(isolate);
  v8::Local<v8::ObjectTemplate> instance_template =
      func_template->InstanceTemplate();

  TestApiCallbacks accessors;
  v8::Local<v8::External> data =
      v8::External::New(isolate, &accessors);
  instance_template->SetAccessor(v8_str("foo"), &TestApiCallbacks::Getter,
                                 &TestApiCallbacks::Setter, data);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env.local()).ToLocalChecked();
  v8::Local<v8::Object> instance =
      func->NewInstance(env.local()).ToLocalChecked();
  env->Global()->Set(env.local(), v8_str("instance"), instance).FromJust();

  CompileRun(sampler_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  int32_t repeat_count = 100;
  v8::Local<v8::Value> args[] = {v8::Integer::New(isolate, repeat_count)};
  RunSampler(env.local(), function, args, arraysize(args), 100, 100);
}

}  // namespace sampler
}  // namespace v8
