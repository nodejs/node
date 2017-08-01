// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_TEST_HELPERS_H_
#define V8_UNITTESTS_TEST_HELPERS_H_

#include <memory>

#include "include/v8.h"
#include "src/compiler-dispatcher/compiler-dispatcher-job.h"
#include "src/parsing/parse-info.h"

namespace v8 {

class Isolate;

namespace internal {

class Object;
template <typename T>
class Handle;
class Isolate;

namespace test {

class ScriptResource : public v8::String::ExternalOneByteStringResource {
 public:
  ScriptResource(const char* data, size_t length)
      : data_(data), length_(length) {}
  ~ScriptResource() override = default;

  const char* data() const override { return data_; }
  size_t length() const override { return length_; }

 private:
  const char* data_;
  size_t length_;

  DISALLOW_COPY_AND_ASSIGN(ScriptResource);
};

class FinishCallback : public CompileJobFinishCallback {
 public:
  void ParseFinished(std::unique_ptr<ParseInfo> result) override {
    result_ = std::move(result);
  }
  ParseInfo* result() const { return result_.get(); }

 private:
  std::unique_ptr<ParseInfo> result_;
};

Handle<Object> RunJS(v8::Isolate* isolate, const char* script);
Handle<String> CreateSource(
    Isolate* isolate,
    v8::String::ExternalOneByteStringResource* maybe_resource);
Handle<SharedFunctionInfo> CreateSharedFunctionInfo(
    Isolate* isolate,
    v8::String::ExternalOneByteStringResource* maybe_resource);

}  // namespace test
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_TEST_HELPERS_H_
