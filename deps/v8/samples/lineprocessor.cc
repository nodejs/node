// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <include/v8.h>

#include <include/libplatform/libplatform.h>
#include <include/v8-debug.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * This sample program should demonstrate certain aspects of debugging
 * standalone V8-based application.
 *
 * The program reads input stream, processes it line by line and print
 * the result to output. The actual processing is done by custom JavaScript
 * script. The script is specified with command line parameters.
 *
 * The main cycle of the program will sequentially read lines from standard
 * input, process them and print to standard output until input closes.
 * There are 2 possible configuration in regard to main cycle.
 *
 * 1. The main cycle is on C++ side. Program should be run with
 * --main-cycle-in-cpp option. Script must declare a function named
 * "ProcessLine". The main cycle in C++ reads lines and calls this function
 * for processing every time. This is a sample script:

function ProcessLine(input_line) {
  return ">>>" + input_line + "<<<";
}

 *
 * 2. The main cycle is in JavaScript. Program should be run with
 * --main-cycle-in-js option. Script gets run one time at all and gets
 * API of 2 global functions: "read_line" and "print". It should read input
 * and print converted lines to output itself. This a sample script:

while (true) {
  var line = read_line();
  if (!line) {
    break;
  }
  var res = line + " | " + line;
  print(res);
}
 */

enum MainCycleType {
  CycleInCpp,
  CycleInJs
};

const char* ToCString(const v8::String::Utf8Value& value);
void ReportException(v8::Isolate* isolate, v8::TryCatch* handler);
v8::Handle<v8::String> ReadFile(v8::Isolate* isolate, const char* name);
v8::Handle<v8::String> ReadLine();

void Print(const v8::FunctionCallbackInfo<v8::Value>& args);
void ReadLine(const v8::FunctionCallbackInfo<v8::Value>& args);
bool RunCppCycle(v8::Handle<v8::Script> script,
                 v8::Local<v8::Context> context,
                 bool report_exceptions);


v8::Persistent<v8::Context> debug_message_context;

int RunMain(int argc, char* argv[]) {
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::Isolate* isolate = v8::Isolate::New();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::Handle<v8::String> script_source;
  v8::Handle<v8::Value> script_name;
  int script_param_counter = 0;

  MainCycleType cycle_type = CycleInCpp;

  for (int i = 1; i < argc; i++) {
    const char* str = argv[i];
    if (strcmp(str, "-f") == 0) {
      // Ignore any -f flags for compatibility with the other stand-
      // alone JavaScript engines.
      continue;
    } else if (strcmp(str, "--main-cycle-in-cpp") == 0) {
      cycle_type = CycleInCpp;
    } else if (strcmp(str, "--main-cycle-in-js") == 0) {
      cycle_type = CycleInJs;
    } else if (strncmp(str, "--", 2) == 0) {
      printf("Warning: unknown flag %s.\nTry --help for options\n", str);
    } else if (strcmp(str, "-e") == 0 && i + 1 < argc) {
      script_source = v8::String::NewFromUtf8(isolate, argv[i + 1]);
      script_name = v8::String::NewFromUtf8(isolate, "unnamed");
      i++;
      script_param_counter++;
    } else {
      // Use argument as a name of file to load.
      script_source = ReadFile(isolate, str);
      script_name = v8::String::NewFromUtf8(isolate, str);
      if (script_source.IsEmpty()) {
        printf("Error reading '%s'\n", str);
        return 1;
      }
      script_param_counter++;
    }
  }

  if (script_param_counter == 0) {
    printf("Script is not specified\n");
    return 1;
  }
  if (script_param_counter != 1) {
    printf("Only one script may be specified\n");
    return 1;
  }

  // Create a template for the global object.
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

  // Bind the global 'print' function to the C++ Print callback.
  global->Set(v8::String::NewFromUtf8(isolate, "print"),
              v8::FunctionTemplate::New(isolate, Print));

  if (cycle_type == CycleInJs) {
    // Bind the global 'read_line' function to the C++ Print callback.
    global->Set(v8::String::NewFromUtf8(isolate, "read_line"),
                v8::FunctionTemplate::New(isolate, ReadLine));
  }

  // Create a new execution environment containing the built-in
  // functions
  v8::Handle<v8::Context> context = v8::Context::New(isolate, NULL, global);
  // Enter the newly created execution environment.
  v8::Context::Scope context_scope(context);

  debug_message_context.Reset(isolate, context);

  bool report_exceptions = true;

  v8::Handle<v8::Script> script;
  {
    // Compile script in try/catch context.
    v8::TryCatch try_catch;
    v8::ScriptOrigin origin(script_name);
    script = v8::Script::Compile(script_source, &origin);
    if (script.IsEmpty()) {
      // Print errors that happened during compilation.
      if (report_exceptions)
        ReportException(isolate, &try_catch);
      return 1;
    }
  }

  {
    v8::TryCatch try_catch;

    script->Run();
    if (try_catch.HasCaught()) {
      if (report_exceptions)
        ReportException(isolate, &try_catch);
      return 1;
    }
  }

  if (cycle_type == CycleInCpp) {
    bool res = RunCppCycle(script,
                           isolate->GetCurrentContext(),
                           report_exceptions);
    return !res;
  } else {
    // All is already done.
  }
  return 0;
}


bool RunCppCycle(v8::Handle<v8::Script> script,
                 v8::Local<v8::Context> context,
                 bool report_exceptions) {
  v8::Isolate* isolate = context->GetIsolate();

  v8::Handle<v8::String> fun_name =
      v8::String::NewFromUtf8(isolate, "ProcessLine");
  v8::Handle<v8::Value> process_val = context->Global()->Get(fun_name);

  // If there is no Process function, or if it is not a function,
  // bail out
  if (!process_val->IsFunction()) {
    printf("Error: Script does not declare 'ProcessLine' global function.\n");
    return 1;
  }

  // It is a function; cast it to a Function
  v8::Handle<v8::Function> process_fun =
      v8::Handle<v8::Function>::Cast(process_val);


  while (!feof(stdin)) {
    v8::HandleScope handle_scope(isolate);

    v8::Handle<v8::String> input_line = ReadLine();
    if (input_line == v8::Undefined(isolate)) {
      continue;
    }

    const int argc = 1;
    v8::Handle<v8::Value> argv[argc] = { input_line };

    v8::Handle<v8::Value> result;
    {
      v8::TryCatch try_catch;
      result = process_fun->Call(isolate->GetCurrentContext()->Global(),
                                 argc, argv);
      if (try_catch.HasCaught()) {
        if (report_exceptions)
          ReportException(isolate, &try_catch);
        return false;
      }
    }
    v8::String::Utf8Value str(result);
    const char* cstr = ToCString(str);
    printf("%s\n", cstr);
  }

  return true;
}


int main(int argc, char* argv[]) {
  v8::V8::InitializeICU();
  v8::Platform* platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  int result = RunMain(argc, argv);
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  delete platform;
  return result;
}


// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}


// Reads a file into a v8 string.
v8::Handle<v8::String> ReadFile(v8::Isolate* isolate, const char* name) {
  FILE* file = fopen(name, "rb");
  if (file == NULL) return v8::Handle<v8::String>();

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = static_cast<int>(fread(&chars[i], 1, size - i, file));
    i += read;
  }
  fclose(file);
  v8::Handle<v8::String> result =
      v8::String::NewFromUtf8(isolate, chars, v8::String::kNormalString, size);
  delete[] chars;
  return result;
}


void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch) {
  v8::HandleScope handle_scope(isolate);
  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);
  v8::Handle<v8::Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    printf("%s\n", exception_string);
  } else {
    // Print (filename):(line number): (message).
    v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    printf("%s:%i: %s\n", filename_string, linenum, exception_string);
    // Print line of source code.
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);
    printf("%s\n", sourceline_string);
    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      printf(" ");
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      printf("^");
    }
    printf("\n");
  }
}


// The callback that is invoked by v8 whenever the JavaScript 'print'
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(args.GetIsolate());
    if (first) {
      first = false;
    } else {
      printf(" ");
    }
    v8::String::Utf8Value str(args[i]);
    const char* cstr = ToCString(str);
    printf("%s", cstr);
  }
  printf("\n");
  fflush(stdout);
}


// The callback that is invoked by v8 whenever the JavaScript 'read_line'
// function is called. Reads a string from standard input and returns.
void ReadLine(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() > 0) {
    args.GetIsolate()->ThrowException(
        v8::String::NewFromUtf8(args.GetIsolate(), "Unexpected arguments"));
    return;
  }
  args.GetReturnValue().Set(ReadLine());
}


v8::Handle<v8::String> ReadLine() {
  const int kBufferSize = 1024 + 1;
  char buffer[kBufferSize];

  char* res;
  {
    res = fgets(buffer, kBufferSize, stdin);
  }
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (res == NULL) {
    v8::Handle<v8::Primitive> t = v8::Undefined(isolate);
    return v8::Handle<v8::String>::Cast(t);
  }
  // Remove newline char
  for (char* pos = buffer; *pos != '\0'; pos++) {
    if (*pos == '\n') {
      *pos = '\0';
      break;
    }
  }
  return v8::String::NewFromUtf8(isolate, buffer);
}
