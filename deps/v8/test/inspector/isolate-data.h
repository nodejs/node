// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_PROTOCOL_ISOLATE_DATA_H_
#define V8_TEST_INSPECTOR_PROTOCOL_ISOLATE_DATA_H_

#include <map>

#include "include/v8-inspector.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/vector.h"
#include "test/inspector/inspector-impl.h"

class TaskRunner;

class IsolateData {
 public:
  class SetupGlobalTask {
   public:
    virtual ~SetupGlobalTask() = default;
    virtual void Run(v8::Isolate* isolate,
                     v8::Local<v8::ObjectTemplate> global) = 0;
  };
  using SetupGlobalTasks = std::vector<std::unique_ptr<SetupGlobalTask>>;

  IsolateData(TaskRunner* task_runner, SetupGlobalTasks setup_global_tasks,
              v8::StartupData* startup_data,
              InspectorClientImpl::FrontendChannel* channel);
  static IsolateData* FromContext(v8::Local<v8::Context> context);

  v8::Isolate* isolate() const { return isolate_; }
  InspectorClientImpl* inspector() const { return inspector_.get(); }
  TaskRunner* task_runner() const { return task_runner_; }
  int CreateContextGroup();
  v8::Local<v8::Context> GetContext(int context_group_id);
  int GetContextGroupId(v8::Local<v8::Context> context);
  void RegisterModule(v8::Local<v8::Context> context,
                      v8::internal::Vector<uint16_t> name,
                      v8::ScriptCompiler::Source* source);
  void FreeContext(v8::Local<v8::Context> context);

 private:
  struct VectorCompare {
    bool operator()(const v8::internal::Vector<uint16_t>& lhs,
                    const v8::internal::Vector<uint16_t>& rhs) const {
      for (int i = 0; i < lhs.length() && i < rhs.length(); ++i) {
        if (lhs[i] != rhs[i]) return lhs[i] < rhs[i];
      }
      return false;
    }
  };
  static v8::MaybeLocal<v8::Module> ModuleResolveCallback(
      v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
      v8::Local<v8::Module> referrer);

  TaskRunner* task_runner_;
  SetupGlobalTasks setup_global_tasks_;
  v8::Isolate* isolate_;
  std::unique_ptr<InspectorClientImpl> inspector_;
  int last_context_group_id_ = 0;
  std::map<int, v8::Global<v8::Context>> contexts_;
  std::map<v8::internal::Vector<uint16_t>, v8::Global<v8::Module>,
           VectorCompare>
      modules_;
};
#endif  //  V8_TEST_INSPECTOR_PROTOCOL_ISOLATE_DATA_H_
