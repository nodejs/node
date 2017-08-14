// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/isolate-data.h"

#include "test/inspector/inspector-impl.h"
#include "test/inspector/task-runner.h"

namespace {

const int kIsolateDataIndex = 2;
const int kContextGroupIdIndex = 3;

v8::internal::Vector<uint16_t> ToVector(v8::Local<v8::String> str) {
  v8::internal::Vector<uint16_t> buffer =
      v8::internal::Vector<uint16_t>::New(str->Length());
  str->Write(buffer.start(), 0, str->Length());
  return buffer;
}

}  //  namespace

IsolateData::IsolateData(TaskRunner* task_runner,
                         IsolateData::SetupGlobalTasks setup_global_tasks,
                         v8::StartupData* startup_data,
                         InspectorClientImpl::FrontendChannel* channel)
    : task_runner_(task_runner),
      setup_global_tasks_(std::move(setup_global_tasks)) {
  v8::Isolate::CreateParams params;
  params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  params.snapshot_blob = startup_data;
  isolate_ = v8::Isolate::New(params);
  isolate_->SetMicrotasksPolicy(v8::MicrotasksPolicy::kScoped);
  if (channel)
    inspector_.reset(new InspectorClientImpl(isolate_, task_runner, channel));
}

IsolateData* IsolateData::FromContext(v8::Local<v8::Context> context) {
  return static_cast<IsolateData*>(
      context->GetAlignedPointerFromEmbedderData(kIsolateDataIndex));
}

int IsolateData::CreateContextGroup() {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate_);
  for (auto it = setup_global_tasks_.begin(); it != setup_global_tasks_.end();
       ++it) {
    (*it)->Run(isolate_, global_template);
  }
  v8::Local<v8::Context> context =
      v8::Context::New(isolate_, nullptr, global_template);
  context->SetAlignedPointerInEmbedderData(kIsolateDataIndex, this);
  int context_group_id = ++last_context_group_id_;
  // Should be 2-byte aligned.
  context->SetAlignedPointerInEmbedderData(
      kContextGroupIdIndex, reinterpret_cast<void*>(context_group_id * 2));
  contexts_[context_group_id].Reset(isolate_, context);
  if (inspector_) inspector_->ContextCreated(context, context_group_id);
  return context_group_id;
}

v8::Local<v8::Context> IsolateData::GetContext(int context_group_id) {
  return contexts_[context_group_id].Get(isolate_);
}

int IsolateData::GetContextGroupId(v8::Local<v8::Context> context) {
  return static_cast<int>(
      reinterpret_cast<intptr_t>(
          context->GetAlignedPointerFromEmbedderData(kContextGroupIdIndex)) /
      2);
}

void IsolateData::RegisterModule(v8::Local<v8::Context> context,
                                 v8::internal::Vector<uint16_t> name,
                                 v8::ScriptCompiler::Source* source) {
  v8::Local<v8::Module> module;
  if (!v8::ScriptCompiler::CompileModule(isolate(), source).ToLocal(&module))
    return;
  if (!module->Instantiate(context, &IsolateData::ModuleResolveCallback))
    return;
  v8::Local<v8::Value> result;
  if (!module->Evaluate(context).ToLocal(&result)) return;
  modules_[name] = v8::Global<v8::Module>(isolate_, module);
}

v8::MaybeLocal<v8::Module> IsolateData::ModuleResolveCallback(
    v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
    v8::Local<v8::Module> referrer) {
  std::string str = *v8::String::Utf8Value(specifier);
  IsolateData* data = IsolateData::FromContext(context);
  return data->modules_[ToVector(specifier)].Get(data->isolate_);
}

void IsolateData::FreeContext(v8::Local<v8::Context> context) {
  int context_group_id = GetContextGroupId(context);
  auto it = contexts_.find(context_group_id);
  if (it == contexts_.end()) return;
  contexts_.erase(it);
}
