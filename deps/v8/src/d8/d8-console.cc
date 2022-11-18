// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8/d8-console.h"

#include "src/execution/isolate.h"

namespace v8 {

namespace {
void WriteToFile(const char* prefix, FILE* file, Isolate* isolate,
                 const debug::ConsoleCallArguments& args) {
  if (prefix) fprintf(file, "%s: ", prefix);
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope(isolate);
    if (i > 0) fprintf(file, " ");

    Local<Value> arg = args[i];
    Local<String> str_obj;

    if (arg->IsSymbol()) arg = Local<Symbol>::Cast(arg)->Description(isolate);
    if (!arg->ToString(isolate->GetCurrentContext()).ToLocal(&str_obj)) return;

    v8::String::Utf8Value str(isolate, str_obj);
    int n = static_cast<int>(fwrite(*str, sizeof(**str), str.length(), file));
    if (n != str.length()) {
      printf("Error in fwrite\n");
      base::OS::ExitProcess(1);
    }
  }
  fprintf(file, "\n");
}
}  // anonymous namespace

D8Console::D8Console(Isolate* isolate) : isolate_(isolate) {
  default_timer_ = base::TimeTicks::Now();
}

void D8Console::Assert(const debug::ConsoleCallArguments& args,
                       const v8::debug::ConsoleContext&) {
  // If no arguments given, the "first" argument is undefined which is
  // false-ish.
  if (args.Length() > 0 && args[0]->BooleanValue(isolate_)) return;
  WriteToFile("console.assert", stdout, isolate_, args);
  isolate_->ThrowError("console.assert failed");
}

void D8Console::Log(const debug::ConsoleCallArguments& args,
                    const v8::debug::ConsoleContext&) {
  WriteToFile(nullptr, stdout, isolate_, args);
}

void D8Console::Error(const debug::ConsoleCallArguments& args,
                      const v8::debug::ConsoleContext&) {
  WriteToFile("console.error", stderr, isolate_, args);
}

void D8Console::Warn(const debug::ConsoleCallArguments& args,
                     const v8::debug::ConsoleContext&) {
  WriteToFile("console.warn", stdout, isolate_, args);
}

void D8Console::Info(const debug::ConsoleCallArguments& args,
                     const v8::debug::ConsoleContext&) {
  WriteToFile("console.info", stdout, isolate_, args);
}

void D8Console::Debug(const debug::ConsoleCallArguments& args,
                      const v8::debug::ConsoleContext&) {
  WriteToFile("console.debug", stdout, isolate_, args);
}

void D8Console::Time(const debug::ConsoleCallArguments& args,
                     const v8::debug::ConsoleContext&) {
  if (i::v8_flags.correctness_fuzzer_suppressions) return;
  if (args.Length() == 0) {
    default_timer_ = base::TimeTicks::Now();
  } else {
    Local<Value> arg = args[0];
    Local<String> label;
    v8::TryCatch try_catch(isolate_);
    if (!arg->ToString(isolate_->GetCurrentContext()).ToLocal(&label)) return;
    v8::String::Utf8Value utf8(isolate_, label);
    std::string string(*utf8);
    auto find = timers_.find(string);
    if (find != timers_.end()) {
      find->second = base::TimeTicks::Now();
    } else {
      timers_.insert(std::pair<std::string, base::TimeTicks>(
          string, base::TimeTicks::Now()));
    }
  }
}

void D8Console::TimeEnd(const debug::ConsoleCallArguments& args,
                        const v8::debug::ConsoleContext&) {
  if (i::v8_flags.correctness_fuzzer_suppressions) return;
  base::TimeDelta delta;
  if (args.Length() == 0) {
    delta = base::TimeTicks::Now() - default_timer_;
    printf("console.timeEnd: default, %f\n", delta.InMillisecondsF());
  } else {
    base::TimeTicks now = base::TimeTicks::Now();
    Local<Value> arg = args[0];
    Local<String> label;
    v8::TryCatch try_catch(isolate_);
    if (!arg->ToString(isolate_->GetCurrentContext()).ToLocal(&label)) return;
    v8::String::Utf8Value utf8(isolate_, label);
    std::string string(*utf8);
    auto find = timers_.find(string);
    if (find != timers_.end()) {
      delta = now - find->second;
      timers_.erase(find);
    }
    printf("console.timeEnd: %s, %f\n", *utf8, delta.InMillisecondsF());
  }
}

void D8Console::TimeStamp(const debug::ConsoleCallArguments& args,
                          const v8::debug::ConsoleContext&) {
  if (i::v8_flags.correctness_fuzzer_suppressions) return;
  base::TimeDelta delta = base::TimeTicks::Now() - default_timer_;
  if (args.Length() == 0) {
    printf("console.timeStamp: default, %f\n", delta.InMillisecondsF());
  } else {
    Local<Value> arg = args[0];
    Local<String> label;
    v8::TryCatch try_catch(isolate_);
    if (!arg->ToString(isolate_->GetCurrentContext()).ToLocal(&label)) return;
    v8::String::Utf8Value utf8(isolate_, label);
    std::string string(*utf8);
    printf("console.timeStamp: %s, %f\n", *utf8, delta.InMillisecondsF());
  }
}

void D8Console::Trace(const debug::ConsoleCallArguments& args,
                      const v8::debug::ConsoleContext&) {
  if (i::v8_flags.correctness_fuzzer_suppressions) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate_);
  i_isolate->PrintStack(stderr, i::Isolate::kPrintStackConcise);
}

}  // namespace v8
