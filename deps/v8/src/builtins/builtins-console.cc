// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"

#include "src/debug/interface-types.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Console

#define CONSOLE_METHOD_LIST(V)      \
  V(Debug, debug)                   \
  V(Error, error)                   \
  V(Info, info)                     \
  V(Log, log)                       \
  V(Warn, warn)                     \
  V(Dir, dir)                       \
  V(DirXml, dirXml)                 \
  V(Table, table)                   \
  V(Trace, trace)                   \
  V(Group, group)                   \
  V(GroupCollapsed, groupCollapsed) \
  V(GroupEnd, groupEnd)             \
  V(Clear, clear)                   \
  V(Count, count)                   \
  V(Assert, assert)                 \
  V(MarkTimeline, markTimeline)     \
  V(Profile, profile)               \
  V(ProfileEnd, profileEnd)         \
  V(Timeline, timeline)             \
  V(TimelineEnd, timelineEnd)       \
  V(Time, time)                     \
  V(TimeEnd, timeEnd)               \
  V(TimeStamp, timeStamp)

namespace {
void ConsoleCall(
    Isolate* isolate, internal::BuiltinArguments& args,
    void (debug::ConsoleDelegate::*func)(const v8::debug::ConsoleCallArguments&,
                                         const v8::debug::ConsoleContext&)) {
  HandleScope scope(isolate);
  if (!isolate->console_delegate()) return;
  debug::ConsoleCallArguments wrapper(args);
  Handle<Object> context_id_obj = JSObject::GetDataProperty(
      args.target(), isolate->factory()->console_context_id_symbol());
  int context_id =
      context_id_obj->IsSmi() ? Handle<Smi>::cast(context_id_obj)->value() : 0;
  Handle<Object> context_name_obj = JSObject::GetDataProperty(
      args.target(), isolate->factory()->console_context_name_symbol());
  Handle<String> context_name = context_name_obj->IsString()
                                    ? Handle<String>::cast(context_name_obj)
                                    : isolate->factory()->anonymous_string();
  (isolate->console_delegate()->*func)(
      wrapper,
      v8::debug::ConsoleContext(context_id, Utils::ToLocal(context_name)));
  CHECK(!isolate->has_pending_exception());
  CHECK(!isolate->has_scheduled_exception());
}
}  // namespace

#define CONSOLE_BUILTIN_IMPLEMENTATION(call, name)             \
  BUILTIN(Console##call) {                                     \
    ConsoleCall(isolate, args, &debug::ConsoleDelegate::call); \
    return isolate->heap()->undefined_value();                 \
  }
CONSOLE_METHOD_LIST(CONSOLE_BUILTIN_IMPLEMENTATION)
#undef CONSOLE_BUILTIN_IMPLEMENTATION

namespace {
void InstallContextFunction(Handle<JSObject> target, const char* name,
                            Builtins::Name call, int context_id,
                            Handle<Object> context_name) {
  Factory* const factory = target->GetIsolate()->factory();

  Handle<Code> call_code(target->GetIsolate()->builtins()->builtin(call));

  Handle<String> name_string =
      Name::ToFunctionName(factory->InternalizeUtf8String(name))
          .ToHandleChecked();
  Handle<JSFunction> fun =
      factory->NewFunctionWithoutPrototype(name_string, call_code, SLOPPY);
  fun->shared()->set_native(true);
  fun->shared()->DontAdaptArguments();
  fun->shared()->set_length(1);

  JSObject::AddProperty(fun, factory->console_context_id_symbol(),
                        handle(Smi::FromInt(context_id), target->GetIsolate()),
                        NONE);
  if (context_name->IsString()) {
    JSObject::AddProperty(fun, factory->console_context_name_symbol(),
                          context_name, NONE);
  }
  JSObject::AddProperty(target, name_string, fun, NONE);
}
}  // namespace

BUILTIN(ConsoleContext) {
  HandleScope scope(isolate);

  Factory* const factory = isolate->factory();
  Handle<String> name = factory->InternalizeUtf8String("Context");
  Handle<JSFunction> cons = factory->NewFunction(name);
  Handle<JSObject> empty = factory->NewJSObject(isolate->object_function());
  JSFunction::SetPrototype(cons, empty);
  Handle<JSObject> context = factory->NewJSObject(cons, TENURED);
  DCHECK(context->IsJSObject());
  int id = isolate->last_console_context_id() + 1;
  isolate->set_last_console_context_id(id);

#define CONSOLE_BUILTIN_SETUP(call, name)                              \
  InstallContextFunction(context, #name, Builtins::kConsole##call, id, \
                         args.at(1));
  CONSOLE_METHOD_LIST(CONSOLE_BUILTIN_SETUP)
#undef CONSOLE_BUILTIN_SETUP

  return *context;
}

#undef CONSOLE_METHOD_LIST

}  // namespace internal
}  // namespace v8
