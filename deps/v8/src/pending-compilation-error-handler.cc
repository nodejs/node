// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/pending-compilation-error-handler.h"

#include "src/ast/ast-value-factory.h"
#include "src/debug/debug.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/messages.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

Handle<String> PendingCompilationErrorHandler::ArgumentString(
    Isolate* isolate) {
  if (arg_ != NULL) return arg_->string();
  if (char_arg_ != NULL) {
    return isolate->factory()
        ->NewStringFromUtf8(CStrVector(char_arg_))
        .ToHandleChecked();
  }
  return isolate->factory()->undefined_string();
}

Handle<String> PendingCompilationErrorHandler::FormatMessage(Isolate* isolate) {
  return MessageTemplate::FormatMessage(isolate, message_,
                                        ArgumentString(isolate));
}

void PendingCompilationErrorHandler::ThrowPendingError(Isolate* isolate,
                                                       Handle<Script> script) {
  if (!has_pending_error_) return;
  MessageLocation location(script, start_position_, end_position_);
  Factory* factory = isolate->factory();
  Handle<String> argument = ArgumentString(isolate);
  isolate->debug()->OnCompileError(script);

  Handle<Object> error;
  switch (error_type_) {
    case kReferenceError:
      error = factory->NewReferenceError(message_, argument);
      break;
    case kSyntaxError:
      error = factory->NewSyntaxError(message_, argument);
      break;
    default:
      UNREACHABLE();
      break;
  }

  if (!error->IsJSObject()) {
    isolate->Throw(*error, &location);
    return;
  }

  Handle<JSObject> jserror = Handle<JSObject>::cast(error);

  Handle<Name> key_start_pos = factory->error_start_pos_symbol();
  JSObject::SetProperty(jserror, key_start_pos,
                        handle(Smi::FromInt(location.start_pos()), isolate),
                        SLOPPY).Check();

  Handle<Name> key_end_pos = factory->error_end_pos_symbol();
  JSObject::SetProperty(jserror, key_end_pos,
                        handle(Smi::FromInt(location.end_pos()), isolate),
                        SLOPPY).Check();

  Handle<Name> key_script = factory->error_script_symbol();
  JSObject::SetProperty(jserror, key_script, script, SLOPPY).Check();

  isolate->Throw(*error, &location);
}
}  // namespace internal
}  // namespace v8
