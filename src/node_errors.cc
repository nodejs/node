#include <stdarg.h>
#include "node_errors.h"
#include "node_internals.h"

namespace node {
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Message;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::ScriptOrigin;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;

bool IsExceptionDecorated(Environment* env, Local<Value> er) {
  if (!er.IsEmpty() && er->IsObject()) {
    Local<Object> err_obj = er.As<Object>();
    auto maybe_value =
        err_obj->GetPrivate(env->context(), env->decorated_private_symbol());
    Local<Value> decorated;
    return maybe_value.ToLocal(&decorated) && decorated->IsTrue();
  }
  return false;
}

void AppendExceptionLine(Environment* env,
                         Local<Value> er,
                         Local<Message> message,
                         enum ErrorHandlingMode mode) {
  if (message.IsEmpty()) return;

  HandleScope scope(env->isolate());
  Local<Object> err_obj;
  if (!er.IsEmpty() && er->IsObject()) {
    err_obj = er.As<Object>();
  }

  // Print (filename):(line number): (message).
  ScriptOrigin origin = message->GetScriptOrigin();
  node::Utf8Value filename(env->isolate(), message->GetScriptResourceName());
  const char* filename_string = *filename;
  int linenum = message->GetLineNumber(env->context()).FromJust();
  // Print line of source code.
  MaybeLocal<String> source_line_maybe = message->GetSourceLine(env->context());
  node::Utf8Value sourceline(env->isolate(),
                             source_line_maybe.ToLocalChecked());
  const char* sourceline_string = *sourceline;
  if (strstr(sourceline_string, "node-do-not-add-exception-line") != nullptr)
    return;

  // Because of how node modules work, all scripts are wrapped with a
  // "function (module, exports, __filename, ...) {"
  // to provide script local variables.
  //
  // When reporting errors on the first line of a script, this wrapper
  // function is leaked to the user. There used to be a hack here to
  // truncate off the first 62 characters, but it caused numerous other
  // problems when vm.runIn*Context() methods were used for non-module
  // code.
  //
  // If we ever decide to re-instate such a hack, the following steps
  // must be taken:
  //
  // 1. Pass a flag around to say "this code was wrapped"
  // 2. Update the stack frame output so that it is also correct.
  //
  // It would probably be simpler to add a line rather than add some
  // number of characters to the first line, since V8 truncates the
  // sourceline to 78 characters, and we end up not providing very much
  // useful debugging info to the user if we remove 62 characters.

  int script_start = (linenum - origin.ResourceLineOffset()->Value()) == 1
                         ? origin.ResourceColumnOffset()->Value()
                         : 0;
  int start = message->GetStartColumn(env->context()).FromMaybe(0);
  int end = message->GetEndColumn(env->context()).FromMaybe(0);
  if (start >= script_start) {
    CHECK_GE(end, start);
    start -= script_start;
    end -= script_start;
  }

  char arrow[1024];
  int max_off = sizeof(arrow) - 2;

  int off = snprintf(arrow,
                     sizeof(arrow),
                     "%s:%i\n%s\n",
                     filename_string,
                     linenum,
                     sourceline_string);
  CHECK_GE(off, 0);
  if (off > max_off) {
    off = max_off;
  }

  // Print wavy underline (GetUnderline is deprecated).
  for (int i = 0; i < start; i++) {
    if (sourceline_string[i] == '\0' || off >= max_off) {
      break;
    }
    CHECK_LT(off, max_off);
    arrow[off++] = (sourceline_string[i] == '\t') ? '\t' : ' ';
  }
  for (int i = start; i < end; i++) {
    if (sourceline_string[i] == '\0' || off >= max_off) {
      break;
    }
    CHECK_LT(off, max_off);
    arrow[off++] = '^';
  }
  CHECK_LE(off, max_off);
  arrow[off] = '\n';
  arrow[off + 1] = '\0';

  Local<String> arrow_str =
      String::NewFromUtf8(env->isolate(), arrow, NewStringType::kNormal)
          .ToLocalChecked();

  const bool can_set_arrow = !arrow_str.IsEmpty() && !err_obj.IsEmpty();
  // If allocating arrow_str failed, print it out. There's not much else to do.
  // If it's not an error, but something needs to be printed out because
  // it's a fatal exception, also print it out from here.
  // Otherwise, the arrow property will be attached to the object and handled
  // by the caller.
  if (!can_set_arrow || (mode == FATAL_ERROR && !err_obj->IsNativeError())) {
    if (env->printed_error()) return;
    Mutex::ScopedLock lock(process_mutex);
    env->set_printed_error(true);

    uv_tty_reset_mode();
    PrintErrorString("\n%s", arrow);
    return;
  }

  CHECK(err_obj
            ->SetPrivate(
                env->context(), env->arrow_message_private_symbol(), arrow_str)
            .FromMaybe(false));
}

[[noreturn]] void Abort() {
  DumpBacktrace(stderr);
  fflush(stderr);
  ABORT_NO_BACKTRACE();
}

[[noreturn]] void Assert(const char* const (*args)[4]) {
  auto filename = (*args)[0];
  auto linenum = (*args)[1];
  auto message = (*args)[2];
  auto function = (*args)[3];

  char name[1024];
  GetHumanReadableProcessName(&name);

  fprintf(stderr,
          "%s: %s:%s:%s%s Assertion `%s' failed.\n",
          name,
          filename,
          linenum,
          function,
          *function ? ":" : "",
          message);
  fflush(stderr);

  Abort();
}

void ReportException(Environment* env,
                     Local<Value> er,
                     Local<Message> message) {
  CHECK(!er.IsEmpty());
  HandleScope scope(env->isolate());

  if (message.IsEmpty()) message = Exception::CreateMessage(env->isolate(), er);

  AppendExceptionLine(env, er, message, FATAL_ERROR);

  Local<Value> trace_value;
  Local<Value> arrow;
  const bool decorated = IsExceptionDecorated(env, er);

  if (er->IsUndefined() || er->IsNull()) {
    trace_value = Undefined(env->isolate());
  } else {
    Local<Object> err_obj = er->ToObject(env->context()).ToLocalChecked();

    trace_value = err_obj->Get(env->stack_string());
    arrow =
        err_obj->GetPrivate(env->context(), env->arrow_message_private_symbol())
            .ToLocalChecked();
  }

  node::Utf8Value trace(env->isolate(), trace_value);

  // range errors have a trace member set to undefined
  if (trace.length() > 0 && !trace_value->IsUndefined()) {
    if (arrow.IsEmpty() || !arrow->IsString() || decorated) {
      PrintErrorString("%s\n", *trace);
    } else {
      node::Utf8Value arrow_string(env->isolate(), arrow);
      PrintErrorString("%s\n%s\n", *arrow_string, *trace);
    }
  } else {
    // this really only happens for RangeErrors, since they're the only
    // kind that won't have all this info in the trace, or when non-Error
    // objects are thrown manually.
    Local<Value> message;
    Local<Value> name;

    if (er->IsObject()) {
      Local<Object> err_obj = er.As<Object>();
      message = err_obj->Get(env->message_string());
      name = err_obj->Get(FIXED_ONE_BYTE_STRING(env->isolate(), "name"));
    }

    if (message.IsEmpty() || message->IsUndefined() || name.IsEmpty() ||
        name->IsUndefined()) {
      // Not an error object. Just print as-is.
      String::Utf8Value message(env->isolate(), er);

      PrintErrorString("%s\n",
                       *message ? *message : "<toString() threw exception>");
    } else {
      node::Utf8Value name_string(env->isolate(), name);
      node::Utf8Value message_string(env->isolate(), message);

      if (arrow.IsEmpty() || !arrow->IsString() || decorated) {
        PrintErrorString("%s: %s\n", *name_string, *message_string);
      } else {
        node::Utf8Value arrow_string(env->isolate(), arrow);
        PrintErrorString(
            "%s\n%s: %s\n", *arrow_string, *name_string, *message_string);
      }
    }
  }

  fflush(stderr);

#if HAVE_INSPECTOR
  env->inspector_agent()->FatalException(er, message);
#endif
}

void ReportException(Environment* env, const TryCatch& try_catch) {
  ReportException(env, try_catch.Exception(), try_catch.Message());
}

void DecorateErrorStack(Environment* env, const TryCatch& try_catch) {
  Local<Value> exception = try_catch.Exception();

  if (!exception->IsObject()) return;

  Local<Object> err_obj = exception.As<Object>();

  if (IsExceptionDecorated(env, err_obj)) return;

  AppendExceptionLine(env, exception, try_catch.Message(), CONTEXTIFY_ERROR);
  Local<Value> stack = err_obj->Get(env->stack_string());
  MaybeLocal<Value> maybe_value =
      err_obj->GetPrivate(env->context(), env->arrow_message_private_symbol());

  Local<Value> arrow;
  if (!(maybe_value.ToLocal(&arrow) && arrow->IsString())) {
    return;
  }

  if (stack.IsEmpty() || !stack->IsString()) {
    return;
  }

  Local<String> decorated_stack = String::Concat(
      env->isolate(),
      String::Concat(env->isolate(),
                     arrow.As<String>(),
                     FIXED_ONE_BYTE_STRING(env->isolate(), "\n")),
      stack.As<String>());
  err_obj->Set(env->stack_string(), decorated_stack);
  err_obj->SetPrivate(
      env->context(), env->decorated_private_symbol(), True(env->isolate()));
}

void PrintErrorString(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
#ifdef _WIN32
  HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);

  // Check if stderr is something other than a tty/console
  if (stderr_handle == INVALID_HANDLE_VALUE || stderr_handle == nullptr ||
      uv_guess_handle(_fileno(stderr)) != UV_TTY) {
    vfprintf(stderr, format, ap);
    va_end(ap);
    return;
  }

  // Fill in any placeholders
  int n = _vscprintf(format, ap);
  std::vector<char> out(n + 1);
  vsprintf(out.data(), format, ap);

  // Get required wide buffer size
  n = MultiByteToWideChar(CP_UTF8, 0, out.data(), -1, nullptr, 0);

  std::vector<wchar_t> wbuf(n);
  MultiByteToWideChar(CP_UTF8, 0, out.data(), -1, wbuf.data(), n);

  // Don't include the null character in the output
  CHECK_GT(n, 0);
  WriteConsoleW(stderr_handle, wbuf.data(), n - 1, nullptr, nullptr);
#else
  vfprintf(stderr, format, ap);
#endif
  va_end(ap);
}

[[noreturn]] void FatalError(const char* location, const char* message) {
  OnFatalError(location, message);
  // to suppress compiler warning
  ABORT();
}

void OnFatalError(const char* location, const char* message) {
  if (location) {
    PrintErrorString("FATAL ERROR: %s %s\n", location, message);
  } else {
    PrintErrorString("FATAL ERROR: %s\n", message);
  }
  fflush(stderr);
  ABORT();
}

FatalTryCatch::~FatalTryCatch() {
  if (HasCaught()) {
    HandleScope scope(env_->isolate());
    ReportException(env_, *this);
    exit(7);
  }
}

void FatalException(Isolate* isolate,
                    Local<Value> error,
                    Local<Message> message) {
  HandleScope scope(isolate);

  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);  // TODO(addaleax): Handle nullptr here.
  Local<Object> process_object = env->process_object();
  Local<String> fatal_exception_string = env->fatal_exception_string();
  Local<Value> fatal_exception_function =
      process_object->Get(fatal_exception_string);

  if (!fatal_exception_function->IsFunction()) {
    // Failed before the process._fatalException function was added!
    // this is probably pretty bad.  Nothing to do but report and exit.
    ReportException(env, error, message);
    exit(6);
  } else {
    TryCatch fatal_try_catch(isolate);

    // Do not call FatalException when _fatalException handler throws
    fatal_try_catch.SetVerbose(false);

    // This will return true if the JS layer handled it, false otherwise
    MaybeLocal<Value> caught = fatal_exception_function.As<Function>()->Call(
        env->context(), process_object, 1, &error);

    if (fatal_try_catch.HasTerminated()) return;

    if (fatal_try_catch.HasCaught()) {
      // The fatal exception function threw, so we must exit
      ReportException(env, fatal_try_catch);
      exit(7);
    } else if (caught.ToLocalChecked()->IsFalse()) {
      ReportException(env, error, message);

      // fatal_exception_function call before may have set a new exit code ->
      // read it again, otherwise use default for uncaughtException 1
      Local<String> exit_code = env->exit_code_string();
      Local<Value> code;
      if (!process_object->Get(env->context(), exit_code).ToLocal(&code) ||
          !code->IsInt32()) {
        exit(1);
      }
      exit(code.As<Int32>()->Value());
    }
  }
}

void FatalException(Isolate* isolate, const TryCatch& try_catch) {
  // If we try to print out a termination exception, we'd just get 'null',
  // so just crashing here with that information seems like a better idea,
  // and in particular it seems like we should handle terminations at the call
  // site for this function rather than by printing them out somewhere.
  CHECK(!try_catch.HasTerminated());

  HandleScope scope(isolate);
  if (!try_catch.IsVerbose()) {
    FatalException(isolate, try_catch.Exception(), try_catch.Message());
  }
}

void FatalException(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);
  if (env != nullptr && env->abort_on_uncaught_exception()) {
    Abort();
  }
  Local<Value> exception = args[0];
  Local<Message> message = Exception::CreateMessage(isolate, exception);
  FatalException(isolate, exception, message);
}

}  // namespace node
