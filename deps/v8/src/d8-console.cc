// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8-console.h"
#include "src/d8.h"

namespace v8 {

namespace {
void WriteToFile(FILE* file, Isolate* isolate,
                 const debug::ConsoleCallArguments& args) {
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope(isolate);
    if (i != 0) fprintf(file, " ");

    // Explicitly catch potential exceptions in toString().
    v8::TryCatch try_catch(isolate);
    Local<Value> arg = args[i];
    Local<String> str_obj;

    if (arg->IsSymbol()) arg = Local<Symbol>::Cast(arg)->Name();
    if (!arg->ToString(isolate->GetCurrentContext()).ToLocal(&str_obj)) {
      Shell::ReportException(isolate, &try_catch);
      return;
    }

    v8::String::Utf8Value str(str_obj);
    int n = static_cast<int>(fwrite(*str, sizeof(**str), str.length(), file));
    if (n != str.length()) {
      printf("Error in fwrite\n");
      Shell::Exit(1);
    }
  }
  fprintf(file, "\n");
}
}  // anonymous namespace

D8Console::D8Console(Isolate* isolate) : isolate_(isolate) {
  default_timer_ = base::TimeTicks::HighResolutionNow();
}

void D8Console::Log(const debug::ConsoleCallArguments& args) {
  WriteToFile(stdout, isolate_, args);
}

void D8Console::Error(const debug::ConsoleCallArguments& args) {
  WriteToFile(stderr, isolate_, args);
}

void D8Console::Warn(const debug::ConsoleCallArguments& args) {
  WriteToFile(stdout, isolate_, args);
}

void D8Console::Info(const debug::ConsoleCallArguments& args) {
  WriteToFile(stdout, isolate_, args);
}

void D8Console::Debug(const debug::ConsoleCallArguments& args) {
  WriteToFile(stdout, isolate_, args);
}

void D8Console::Time(const debug::ConsoleCallArguments& args) {
  if (args.Length() == 0) {
    default_timer_ = base::TimeTicks::HighResolutionNow();
  } else {
    Local<Value> arg = args[0];
    Local<String> label;
    v8::TryCatch try_catch(isolate_);
    if (!arg->ToString(isolate_->GetCurrentContext()).ToLocal(&label)) {
      Shell::ReportException(isolate_, &try_catch);
      return;
    }
    v8::String::Utf8Value utf8(label);
    std::string string(*utf8);
    auto find = timers_.find(string);
    if (find != timers_.end()) {
      find->second = base::TimeTicks::HighResolutionNow();
    } else {
      timers_.insert(std::pair<std::string, base::TimeTicks>(
          string, base::TimeTicks::HighResolutionNow()));
    }
  }
}

void D8Console::TimeEnd(const debug::ConsoleCallArguments& args) {
  base::TimeDelta delta;
  base::TimeTicks now = base::TimeTicks::HighResolutionNow();
  if (args.Length() == 0) {
    delta = base::TimeTicks::HighResolutionNow() - default_timer_;
    printf("default: ");
  } else {
    Local<Value> arg = args[0];
    Local<String> label;
    v8::TryCatch try_catch(isolate_);
    if (!arg->ToString(isolate_->GetCurrentContext()).ToLocal(&label)) {
      Shell::ReportException(isolate_, &try_catch);
      return;
    }
    v8::String::Utf8Value utf8(label);
    std::string string(*utf8);
    auto find = timers_.find(string);
    if (find != timers_.end()) {
      delta = now - find->second;
    }
    printf("%s: ", *utf8);
  }
  printf("%f\n", delta.InMillisecondsF());
}

}  // namespace v8
