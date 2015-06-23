// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/pending-compilation-error-handler.h"

#include "src/debug.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/messages.h"

namespace v8 {
namespace internal {

void PendingCompilationErrorHandler::ThrowPendingError(Isolate* isolate,
                                                       Handle<Script> script) {
  if (!has_pending_error_) return;
  MessageLocation location(script, start_position_, end_position_);
  Factory* factory = isolate->factory();
  bool has_arg = arg_ != NULL || char_arg_ != NULL || !handle_arg_.is_null();
  Handle<FixedArray> elements = factory->NewFixedArray(has_arg ? 1 : 0);
  if (arg_ != NULL) {
    Handle<String> arg_string = arg_->string();
    elements->set(0, *arg_string);
  } else if (char_arg_ != NULL) {
    Handle<String> arg_string =
        factory->NewStringFromUtf8(CStrVector(char_arg_)).ToHandleChecked();
    elements->set(0, *arg_string);
  } else if (!handle_arg_.is_null()) {
    elements->set(0, *handle_arg_);
  }
  isolate->debug()->OnCompileError(script);

  Handle<JSArray> array = factory->NewJSArrayWithElements(elements);
  Handle<Object> error;

  switch (error_type_) {
    case kReferenceError:
      error = factory->NewReferenceError(message_, array);
      break;
    case kSyntaxError:
      error = factory->NewSyntaxError(message_, array);
      break;
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
}
}  // namespace v8::internal
