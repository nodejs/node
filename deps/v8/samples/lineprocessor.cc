// Copyright 2009 the V8 project authors. All rights reserved.
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


// This controls whether this sample is compiled with debugger support.
// You may trace its usages in source text to see what parts of program
// are responsible for debugging support.
// Note that V8 itself should be compiled with enabled debugger support
// to have it all working.
#define SUPPORT_DEBUGGING

#include <v8.h>

#ifdef SUPPORT_DEBUGGING
#include <v8-debug.h>
#endif

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

 *
 * When run with "-p" argument, the program starts V8 Debugger Agent and
 * allows remote debugger to attach and debug JavaScript code.
 *
 * Interesting aspects:
 * 1. Wait for remote debugger to attach
 * Normally the program compiles custom script and immediately runs it.
 * If programmer needs to debug script from the very beginning, he should
 * run this sample program with "--wait-for-connection" command line parameter.
 * This way V8 will suspend on the first statement and wait for
 * debugger to attach.
 *
 * 2. Unresponsive V8
 * V8 Debugger Agent holds a connection with remote debugger, but it does
 * respond only when V8 is running some script. In particular, when this program
 * is waiting for input, all requests from debugger get deferred until V8
 * is called again. See how "--callback" command-line parameter in this sample
 * fixes this issue.
 */

enum MainCycleType {
  CycleInCpp,
  CycleInJs
};

const char* ToCString(const v8::String::Utf8Value& value);
void ReportException(v8::TryCatch* handler);
v8::Handle<v8::String> ReadFile(const char* name);
v8::Handle<v8::String> ReadLine();

v8::Handle<v8::Value> Print(const v8::Arguments& args);
v8::Handle<v8::Value> ReadLine(const v8::Arguments& args);
bool RunCppCycle(v8::Handle<v8::Script> script, v8::Local<v8::Context> context,
                 bool report_exceptions);


#ifdef SUPPORT_DEBUGGING
v8::Persistent<v8::Context> debug_message_context;

void DispatchDebugMessages() {
  // We are in some random thread. We should already have v8::Locker acquired
  // (we requested this when registered this callback). We was called
  // because new debug messages arrived; they may have already been processed,
  // but we shouldn't worry about this.
  //
  // All we have to do is to set context and call ProcessDebugMessages.
  //
  // We should decide which V8 context to use here. This is important for
  // "evaluate" command, because it must be executed some context.
  // In our sample we have only one context, so there is nothing really to
  // think about.
  v8::Context::Scope scope(debug_message_context);

  v8::Debug::ProcessDebugMessages();
}
#endif


int RunMain(int argc, char* argv[]) {
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::HandleScope handle_scope;

  v8::Handle<v8::String> script_source(NULL);
  v8::Handle<v8::Value> script_name(NULL);
  int script_param_counter = 0;

#ifdef SUPPORT_DEBUGGING
  int port_number = -1;
  bool wait_for_connection = false;
  bool support_callback = false;
#endif

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
#ifdef SUPPORT_DEBUGGING
    } else if (strcmp(str, "--callback") == 0) {
      support_callback = true;
    } else if (strcmp(str, "--wait-for-connection") == 0) {
      wait_for_connection = true;
    } else if (strcmp(str, "-p") == 0 && i + 1 < argc) {
      port_number = atoi(argv[i + 1]);  // NOLINT
      i++;
#endif
    } else if (strncmp(str, "--", 2) == 0) {
      printf("Warning: unknown flag %s.\nTry --help for options\n", str);
    } else if (strcmp(str, "-e") == 0 && i + 1 < argc) {
      script_source = v8::String::New(argv[i + 1]);
      script_name = v8::String::New("unnamed");
      i++;
      script_param_counter++;
    } else {
      // Use argument as a name of file to load.
      script_source = ReadFile(str);
      script_name = v8::String::New(str);
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
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

  // Bind the global 'print' function to the C++ Print callback.
  global->Set(v8::String::New("print"), v8::FunctionTemplate::New(Print));

  if (cycle_type == CycleInJs) {
    // Bind the global 'read_line' function to the C++ Print callback.
    global->Set(v8::String::New("read_line"),
                v8::FunctionTemplate::New(ReadLine));
  }

  // Create a new execution environment containing the built-in
  // functions
  v8::Handle<v8::Context> context = v8::Context::New(NULL, global);
  // Enter the newly created execution environment.
  v8::Context::Scope context_scope(context);

#ifdef SUPPORT_DEBUGGING
  debug_message_context = v8::Persistent<v8::Context>::New(context);

  v8::Locker locker;

  if (support_callback) {
    v8::Debug::SetDebugMessageDispatchHandler(DispatchDebugMessages, true);
  }

  if (port_number != -1) {
    v8::Debug::EnableAgent("lineprocessor", port_number, wait_for_connection);
  }
#endif

  bool report_exceptions = true;

  v8::Handle<v8::Script> script;
  {
    // Compile script in try/catch context.
    v8::TryCatch try_catch;
    script = v8::Script::Compile(script_source, script_name);
    if (script.IsEmpty()) {
      // Print errors that happened during compilation.
      if (report_exceptions)
        ReportException(&try_catch);
      return 1;
    }
  }

  {
    v8::TryCatch try_catch;

    script->Run();
    if (try_catch.HasCaught()) {
      if (report_exceptions)
        ReportException(&try_catch);
      return 1;
    }
  }

  if (cycle_type == CycleInCpp) {
    bool res = RunCppCycle(script, v8::Context::GetCurrent(),
                           report_exceptions);
    return !res;
  } else {
    // All is already done.
  }
  return 0;
}


bool RunCppCycle(v8::Handle<v8::Script> script, v8::Local<v8::Context> context,
                 bool report_exceptions) {
#ifdef SUPPORT_DEBUGGING
  v8::Locker lock;
#endif

  v8::Handle<v8::String> fun_name = v8::String::New("ProcessLine");
  v8::Handle<v8::Value> process_val =
      v8::Context::GetCurrent()->Global()->Get(fun_name);

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
    v8::HandleScope handle_scope;

    v8::Handle<v8::String> input_line = ReadLine();
    if (input_line == v8::Undefined()) {
      continue;
    }

    const int argc = 1;
    v8::Handle<v8::Value> argv[argc] = { input_line };

    v8::Handle<v8::Value> result;
    {
      v8::TryCatch try_catch;
      result = process_fun->Call(v8::Context::GetCurrent()->Global(),
                                 argc, argv);
      if (try_catch.HasCaught()) {
        if (report_exceptions)
          ReportException(&try_catch);
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
  int result = RunMain(argc, argv);
  v8::V8::Dispose();
  return result;
}


// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}


// Reads a file into a v8 string.
v8::Handle<v8::String> ReadFile(const char* name) {
  FILE* file = fopen(name, "rb");
  if (file == NULL) return v8::Handle<v8::String>();

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = fread(&chars[i], 1, size - i, file);
    i += read;
  }
  fclose(file);
  v8::Handle<v8::String> result = v8::String::New(chars, size);
  delete[] chars;
  return result;
}


void ReportException(v8::TryCatch* try_catch) {
  v8::HandleScope handle_scope;
  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);
  v8::Handle<v8::Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    printf("%s\n", exception_string);
  } else {
    // Print (filename):(line number): (message).
    v8::String::Utf8Value filename(message->GetScriptResourceName());
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
v8::Handle<v8::Value> Print(const v8::Arguments& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope;
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
  return v8::Undefined();
}


// The callback that is invoked by v8 whenever the JavaScript 'read_line'
// function is called. Reads a string from standard input and returns.
v8::Handle<v8::Value> ReadLine(const v8::Arguments& args) {
  if (args.Length() > 0) {
    return v8::ThrowException(v8::String::New("Unexpected arguments"));
  }
  return ReadLine();
}

v8::Handle<v8::String> ReadLine() {
  const int kBufferSize = 1024 + 1;
  char buffer[kBufferSize];

  char* res;
  {
#ifdef SUPPORT_DEBUGGING
    v8::Unlocker unlocker;
#endif
    res = fgets(buffer, kBufferSize, stdin);
  }
  if (res == NULL) {
    v8::Handle<v8::Primitive> t = v8::Undefined();
    return reinterpret_cast<v8::Handle<v8::String>&>(t);
  }
  // remove newline char
  for (char* pos = buffer; *pos != '\0'; pos++) {
    if (*pos == '\n') {
      *pos = '\0';
      break;
    }
  }
  return v8::String::New(buffer);
}
