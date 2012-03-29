// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include <ctype.h>
#include <stdlib.h>

#include "v8.h"

#include "platform.h"
#include "smart-array-pointer.h"
#include "string-stream.h"


namespace v8 {
namespace internal {

// Define all of our flags.
#define FLAG_MODE_DEFINE
#include "flag-definitions.h"

// Define all of our flags default values.
#define FLAG_MODE_DEFINE_DEFAULTS
#include "flag-definitions.h"

namespace {

// This structure represents a single entry in the flag system, with a pointer
// to the actual flag, default value, comment, etc.  This is designed to be POD
// initialized as to avoid requiring static constructors.
struct Flag {
  enum FlagType { TYPE_BOOL, TYPE_INT, TYPE_FLOAT, TYPE_STRING, TYPE_ARGS };

  FlagType type_;           // What type of flag, bool, int, or string.
  const char* name_;        // Name of the flag, ex "my_flag".
  void* valptr_;            // Pointer to the global flag variable.
  const void* defptr_;      // Pointer to the default value.
  const char* cmt_;         // A comment about the flags purpose.
  bool owns_ptr_;           // Does the flag own its string value?

  FlagType type() const { return type_; }

  const char* name() const { return name_; }

  const char* comment() const { return cmt_; }

  bool* bool_variable() const {
    ASSERT(type_ == TYPE_BOOL);
    return reinterpret_cast<bool*>(valptr_);
  }

  int* int_variable() const {
    ASSERT(type_ == TYPE_INT);
    return reinterpret_cast<int*>(valptr_);
  }

  double* float_variable() const {
    ASSERT(type_ == TYPE_FLOAT);
    return reinterpret_cast<double*>(valptr_);
  }

  const char* string_value() const {
    ASSERT(type_ == TYPE_STRING);
    return *reinterpret_cast<const char**>(valptr_);
  }

  void set_string_value(const char* value, bool owns_ptr) {
    ASSERT(type_ == TYPE_STRING);
    const char** ptr = reinterpret_cast<const char**>(valptr_);
    if (owns_ptr_ && *ptr != NULL) DeleteArray(*ptr);
    *ptr = value;
    owns_ptr_ = owns_ptr;
  }

  JSArguments* args_variable() const {
    ASSERT(type_ == TYPE_ARGS);
    return reinterpret_cast<JSArguments*>(valptr_);
  }

  bool bool_default() const {
    ASSERT(type_ == TYPE_BOOL);
    return *reinterpret_cast<const bool*>(defptr_);
  }

  int int_default() const {
    ASSERT(type_ == TYPE_INT);
    return *reinterpret_cast<const int*>(defptr_);
  }

  double float_default() const {
    ASSERT(type_ == TYPE_FLOAT);
    return *reinterpret_cast<const double*>(defptr_);
  }

  const char* string_default() const {
    ASSERT(type_ == TYPE_STRING);
    return *reinterpret_cast<const char* const *>(defptr_);
  }

  JSArguments args_default() const {
    ASSERT(type_ == TYPE_ARGS);
    return *reinterpret_cast<const JSArguments*>(defptr_);
  }

  // Compare this flag's current value against the default.
  bool IsDefault() const {
    switch (type_) {
      case TYPE_BOOL:
        return *bool_variable() == bool_default();
      case TYPE_INT:
        return *int_variable() == int_default();
      case TYPE_FLOAT:
        return *float_variable() == float_default();
      case TYPE_STRING: {
        const char* str1 = string_value();
        const char* str2 = string_default();
        if (str2 == NULL) return str1 == NULL;
        if (str1 == NULL) return str2 == NULL;
        return strcmp(str1, str2) == 0;
      }
      case TYPE_ARGS:
        return args_variable()->argc() == 0;
    }
    UNREACHABLE();
    return true;
  }

  // Set a flag back to it's default value.
  void Reset() {
    switch (type_) {
      case TYPE_BOOL:
        *bool_variable() = bool_default();
        break;
      case TYPE_INT:
        *int_variable() = int_default();
        break;
      case TYPE_FLOAT:
        *float_variable() = float_default();
        break;
      case TYPE_STRING:
        set_string_value(string_default(), false);
        break;
      case TYPE_ARGS:
        *args_variable() = args_default();
        break;
    }
  }
};

Flag flags[] = {
#define FLAG_MODE_META
#include "flag-definitions.h"
};

const size_t num_flags = sizeof(flags) / sizeof(*flags);

}  // namespace


static const char* Type2String(Flag::FlagType type) {
  switch (type) {
    case Flag::TYPE_BOOL: return "bool";
    case Flag::TYPE_INT: return "int";
    case Flag::TYPE_FLOAT: return "float";
    case Flag::TYPE_STRING: return "string";
    case Flag::TYPE_ARGS: return "arguments";
  }
  UNREACHABLE();
  return NULL;
}


static SmartArrayPointer<const char> ToString(Flag* flag) {
  HeapStringAllocator string_allocator;
  StringStream buffer(&string_allocator);
  switch (flag->type()) {
    case Flag::TYPE_BOOL:
      buffer.Add("%s", (*flag->bool_variable() ? "true" : "false"));
      break;
    case Flag::TYPE_INT:
      buffer.Add("%d", *flag->int_variable());
      break;
    case Flag::TYPE_FLOAT:
      buffer.Add("%f", FmtElm(*flag->float_variable()));
      break;
    case Flag::TYPE_STRING: {
      const char* str = flag->string_value();
      buffer.Add("%s", str ? str : "NULL");
      break;
    }
    case Flag::TYPE_ARGS: {
      JSArguments args = *flag->args_variable();
      if (args.argc() > 0) {
        buffer.Add("%s",  args[0]);
        for (int i = 1; i < args.argc(); i++) {
          buffer.Add(" %s", args[i]);
        }
      }
      break;
    }
  }
  return buffer.ToCString();
}


// static
List<const char*>* FlagList::argv() {
  List<const char*>* args = new List<const char*>(8);
  Flag* args_flag = NULL;
  for (size_t i = 0; i < num_flags; ++i) {
    Flag* f = &flags[i];
    if (!f->IsDefault()) {
      if (f->type() == Flag::TYPE_ARGS) {
        ASSERT(args_flag == NULL);
        args_flag = f;  // Must be last in arguments.
        continue;
      }
      HeapStringAllocator string_allocator;
      StringStream buffer(&string_allocator);
      if (f->type() != Flag::TYPE_BOOL || *(f->bool_variable())) {
        buffer.Add("--%s", f->name());
      } else {
        buffer.Add("--no%s", f->name());
      }
      args->Add(buffer.ToCString().Detach());
      if (f->type() != Flag::TYPE_BOOL) {
        args->Add(ToString(f).Detach());
      }
    }
  }
  if (args_flag != NULL) {
    HeapStringAllocator string_allocator;
    StringStream buffer(&string_allocator);
    buffer.Add("--%s", args_flag->name());
    args->Add(buffer.ToCString().Detach());
    JSArguments jsargs = *args_flag->args_variable();
    for (int j = 0; j < jsargs.argc(); j++) {
      args->Add(StrDup(jsargs[j]));
    }
  }
  return args;
}


// Helper function to parse flags: Takes an argument arg and splits it into
// a flag name and flag value (or NULL if they are missing). is_bool is set
// if the arg started with "-no" or "--no". The buffer may be used to NUL-
// terminate the name, it must be large enough to hold any possible name.
static void SplitArgument(const char* arg,
                          char* buffer,
                          int buffer_size,
                          const char** name,
                          const char** value,
                          bool* is_bool) {
  *name = NULL;
  *value = NULL;
  *is_bool = false;

  if (arg != NULL && *arg == '-') {
    // find the begin of the flag name
    arg++;  // remove 1st '-'
    if (*arg == '-') {
      arg++;  // remove 2nd '-'
      if (arg[0] == '\0') {
        const char* kJSArgumentsFlagName = "js_arguments";
        *name = kJSArgumentsFlagName;
        return;
      }
    }
    if (arg[0] == 'n' && arg[1] == 'o') {
      arg += 2;  // remove "no"
      *is_bool = true;
    }
    *name = arg;

    // find the end of the flag name
    while (*arg != '\0' && *arg != '=')
      arg++;

    // get the value if any
    if (*arg == '=') {
      // make a copy so we can NUL-terminate flag name
      size_t n = arg - *name;
      CHECK(n < static_cast<size_t>(buffer_size));  // buffer is too small
      memcpy(buffer, *name, n);
      buffer[n] = '\0';
      *name = buffer;
      // get the value
      *value = arg + 1;
    }
  }
}


inline char NormalizeChar(char ch) {
  return ch == '_' ? '-' : ch;
}


static bool EqualNames(const char* a, const char* b) {
  for (int i = 0; NormalizeChar(a[i]) == NormalizeChar(b[i]); i++) {
    if (a[i] == '\0') {
      return true;
    }
  }
  return false;
}


static Flag* FindFlag(const char* name) {
  for (size_t i = 0; i < num_flags; ++i) {
    if (EqualNames(name, flags[i].name()))
      return &flags[i];
  }
  return NULL;
}


// static
int FlagList::SetFlagsFromCommandLine(int* argc,
                                      char** argv,
                                      bool remove_flags) {
  // parse arguments
  for (int i = 1; i < *argc;) {
    int j = i;  // j > 0
    const char* arg = argv[i++];

    // split arg into flag components
    char buffer[1*KB];
    const char* name;
    const char* value;
    bool is_bool;
    SplitArgument(arg, buffer, sizeof buffer, &name, &value, &is_bool);

    if (name != NULL) {
      // lookup the flag
      Flag* flag = FindFlag(name);
      if (flag == NULL) {
        if (remove_flags) {
          // We don't recognize this flag but since we're removing
          // the flags we recognize we assume that the remaining flags
          // will be processed somewhere else so this flag might make
          // sense there.
          continue;
        } else {
          fprintf(stderr, "Error: unrecognized flag %s\n"
                  "Try --help for options\n", arg);
          return j;
        }
      }

      // if we still need a flag value, use the next argument if available
      if (flag->type() != Flag::TYPE_BOOL &&
          flag->type() != Flag::TYPE_ARGS &&
          value == NULL) {
        if (i < *argc) {
          value = argv[i++];
        } else {
          fprintf(stderr, "Error: missing value for flag %s of type %s\n"
                  "Try --help for options\n",
                  arg, Type2String(flag->type()));
          return j;
        }
      }

      // set the flag
      char* endp = const_cast<char*>("");  // *endp is only read
      switch (flag->type()) {
        case Flag::TYPE_BOOL:
          *flag->bool_variable() = !is_bool;
          break;
        case Flag::TYPE_INT:
          *flag->int_variable() = strtol(value, &endp, 10);  // NOLINT
          break;
        case Flag::TYPE_FLOAT:
          *flag->float_variable() = strtod(value, &endp);
          break;
        case Flag::TYPE_STRING:
          flag->set_string_value(value ? StrDup(value) : NULL, true);
          break;
        case Flag::TYPE_ARGS: {
          int start_pos = (value == NULL) ? i : i - 1;
          int js_argc = *argc - start_pos;
          const char** js_argv = NewArray<const char*>(js_argc);
          if (value != NULL) {
            js_argv[0] = StrDup(value);
          }
          for (int k = i; k < *argc; k++) {
            js_argv[k - start_pos] = StrDup(argv[k]);
          }
          *flag->args_variable() = JSArguments::Create(js_argc, js_argv);
          i = *argc;  // Consume all arguments
          break;
        }
      }

      // handle errors
      if ((flag->type() == Flag::TYPE_BOOL && value != NULL) ||
          (flag->type() != Flag::TYPE_BOOL && is_bool) ||
          *endp != '\0') {
        fprintf(stderr, "Error: illegal value for flag %s of type %s\n"
                "Try --help for options\n",
                arg, Type2String(flag->type()));
        return j;
      }

      // remove the flag & value from the command
      if (remove_flags) {
        while (j < i) {
          argv[j++] = NULL;
        }
      }
    }
  }

  // shrink the argument list
  if (remove_flags) {
    int j = 1;
    for (int i = 1; i < *argc; i++) {
      if (argv[i] != NULL)
        argv[j++] = argv[i];
    }
    *argc = j;
  }

  if (FLAG_help) {
    PrintHelp();
    exit(0);
  }
  // parsed all flags successfully
  return 0;
}


static char* SkipWhiteSpace(char* p) {
  while (*p != '\0' && isspace(*p) != 0) p++;
  return p;
}


static char* SkipBlackSpace(char* p) {
  while (*p != '\0' && isspace(*p) == 0) p++;
  return p;
}


// static
int FlagList::SetFlagsFromString(const char* str, int len) {
  // make a 0-terminated copy of str
  ScopedVector<char> copy0(len + 1);
  memcpy(copy0.start(), str, len);
  copy0[len] = '\0';

  // strip leading white space
  char* copy = SkipWhiteSpace(copy0.start());

  // count the number of 'arguments'
  int argc = 1;  // be compatible with SetFlagsFromCommandLine()
  for (char* p = copy; *p != '\0'; argc++) {
    p = SkipBlackSpace(p);
    p = SkipWhiteSpace(p);
  }

  // allocate argument array
  ScopedVector<char*> argv(argc);

  // split the flags string into arguments
  argc = 1;  // be compatible with SetFlagsFromCommandLine()
  for (char* p = copy; *p != '\0'; argc++) {
    argv[argc] = p;
    p = SkipBlackSpace(p);
    if (*p != '\0') *p++ = '\0';  // 0-terminate argument
    p = SkipWhiteSpace(p);
  }

  // set the flags
  int result = SetFlagsFromCommandLine(&argc, argv.start(), false);

  return result;
}


// static
void FlagList::ResetAllFlags() {
  for (size_t i = 0; i < num_flags; ++i) {
    flags[i].Reset();
  }
}


// static
void FlagList::PrintHelp() {
  printf("Usage:\n");
  printf("  shell [options] -e string\n");
  printf("    execute string in V8\n");
  printf("  shell [options] file1 file2 ... filek\n");
  printf("    run JavaScript scripts in file1, file2, ..., filek\n");
  printf("  shell [options]\n");
  printf("  shell [options] --shell [file1 file2 ... filek]\n");
  printf("    run an interactive JavaScript shell\n");
  printf("  d8 [options] file1 file2 ... filek\n");
  printf("  d8 [options]\n");
  printf("  d8 [options] --shell [file1 file2 ... filek]\n");
  printf("    run the new debugging shell\n\n");
  printf("Options:\n");
  for (size_t i = 0; i < num_flags; ++i) {
    Flag* f = &flags[i];
    SmartArrayPointer<const char> value = ToString(f);
    printf("  --%s (%s)\n        type: %s  default: %s\n",
           f->name(), f->comment(), Type2String(f->type()), *value);
  }
}


void FlagList::EnforceFlagImplications() {
#define FLAG_MODE_DEFINE_IMPLICATIONS
#include "flag-definitions.h"
}

} }  // namespace v8::internal
