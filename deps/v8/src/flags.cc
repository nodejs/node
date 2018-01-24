// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/base/functional.h"
#include "src/base/platform/platform.h"
#include "src/ostreams.h"
#include "src/utils.h"
#include "src/wasm/wasm-limits.h"

namespace v8 {
namespace internal {

// Define all of our flags.
#define FLAG_MODE_DEFINE
#include "src/flag-definitions.h"  // NOLINT(build/include)

// Define all of our flags default values.
#define FLAG_MODE_DEFINE_DEFAULTS
#include "src/flag-definitions.h"  // NOLINT(build/include)

namespace {

// This structure represents a single entry in the flag system, with a pointer
// to the actual flag, default value, comment, etc.  This is designed to be POD
// initialized as to avoid requiring static constructors.
struct Flag {
  enum FlagType {
    TYPE_BOOL,
    TYPE_MAYBE_BOOL,
    TYPE_INT,
    TYPE_UINT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_ARGS
  };

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
    DCHECK(type_ == TYPE_BOOL);
    return reinterpret_cast<bool*>(valptr_);
  }

  MaybeBoolFlag* maybe_bool_variable() const {
    DCHECK(type_ == TYPE_MAYBE_BOOL);
    return reinterpret_cast<MaybeBoolFlag*>(valptr_);
  }

  int* int_variable() const {
    DCHECK(type_ == TYPE_INT);
    return reinterpret_cast<int*>(valptr_);
  }

  unsigned int* uint_variable() const {
    DCHECK(type_ == TYPE_UINT);
    return reinterpret_cast<unsigned int*>(valptr_);
  }

  double* float_variable() const {
    DCHECK(type_ == TYPE_FLOAT);
    return reinterpret_cast<double*>(valptr_);
  }

  const char* string_value() const {
    DCHECK(type_ == TYPE_STRING);
    return *reinterpret_cast<const char**>(valptr_);
  }

  void set_string_value(const char* value, bool owns_ptr) {
    DCHECK(type_ == TYPE_STRING);
    const char** ptr = reinterpret_cast<const char**>(valptr_);
    if (owns_ptr_ && *ptr != nullptr) DeleteArray(*ptr);
    *ptr = value;
    owns_ptr_ = owns_ptr;
  }

  JSArguments* args_variable() const {
    DCHECK(type_ == TYPE_ARGS);
    return reinterpret_cast<JSArguments*>(valptr_);
  }

  bool bool_default() const {
    DCHECK(type_ == TYPE_BOOL);
    return *reinterpret_cast<const bool*>(defptr_);
  }

  int int_default() const {
    DCHECK(type_ == TYPE_INT);
    return *reinterpret_cast<const int*>(defptr_);
  }

  unsigned int uint_default() const {
    DCHECK(type_ == TYPE_UINT);
    return *reinterpret_cast<const unsigned int*>(defptr_);
  }

  double float_default() const {
    DCHECK(type_ == TYPE_FLOAT);
    return *reinterpret_cast<const double*>(defptr_);
  }

  const char* string_default() const {
    DCHECK(type_ == TYPE_STRING);
    return *reinterpret_cast<const char* const *>(defptr_);
  }

  JSArguments args_default() const {
    DCHECK(type_ == TYPE_ARGS);
    return *reinterpret_cast<const JSArguments*>(defptr_);
  }

  // Compare this flag's current value against the default.
  bool IsDefault() const {
    switch (type_) {
      case TYPE_BOOL:
        return *bool_variable() == bool_default();
      case TYPE_MAYBE_BOOL:
        return maybe_bool_variable()->has_value == false;
      case TYPE_INT:
        return *int_variable() == int_default();
      case TYPE_UINT:
        return *uint_variable() == uint_default();
      case TYPE_FLOAT:
        return *float_variable() == float_default();
      case TYPE_STRING: {
        const char* str1 = string_value();
        const char* str2 = string_default();
        if (str2 == nullptr) return str1 == nullptr;
        if (str1 == nullptr) return str2 == nullptr;
        return strcmp(str1, str2) == 0;
      }
      case TYPE_ARGS:
        return args_variable()->argc == 0;
    }
    UNREACHABLE();
  }

  // Set a flag back to it's default value.
  void Reset() {
    switch (type_) {
      case TYPE_BOOL:
        *bool_variable() = bool_default();
        break;
      case TYPE_MAYBE_BOOL:
        *maybe_bool_variable() = MaybeBoolFlag::Create(false, false);
        break;
      case TYPE_INT:
        *int_variable() = int_default();
        break;
      case TYPE_UINT:
        *uint_variable() = uint_default();
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
#include "src/flag-definitions.h"  // NOLINT(build/include)
};

const size_t num_flags = sizeof(flags) / sizeof(*flags);

}  // namespace


static const char* Type2String(Flag::FlagType type) {
  switch (type) {
    case Flag::TYPE_BOOL: return "bool";
    case Flag::TYPE_MAYBE_BOOL: return "maybe_bool";
    case Flag::TYPE_INT: return "int";
    case Flag::TYPE_UINT:
      return "uint";
    case Flag::TYPE_FLOAT: return "float";
    case Flag::TYPE_STRING: return "string";
    case Flag::TYPE_ARGS: return "arguments";
  }
  UNREACHABLE();
}


std::ostream& operator<<(std::ostream& os, const Flag& flag) {  // NOLINT
  switch (flag.type()) {
    case Flag::TYPE_BOOL:
      os << (*flag.bool_variable() ? "true" : "false");
      break;
    case Flag::TYPE_MAYBE_BOOL:
      os << (flag.maybe_bool_variable()->has_value
                 ? (flag.maybe_bool_variable()->value ? "true" : "false")
                 : "unset");
      break;
    case Flag::TYPE_INT:
      os << *flag.int_variable();
      break;
    case Flag::TYPE_UINT:
      os << *flag.uint_variable();
      break;
    case Flag::TYPE_FLOAT:
      os << *flag.float_variable();
      break;
    case Flag::TYPE_STRING: {
      const char* str = flag.string_value();
      os << (str ? str : "nullptr");
      break;
    }
    case Flag::TYPE_ARGS: {
      JSArguments args = *flag.args_variable();
      if (args.argc > 0) {
        os << args[0];
        for (int i = 1; i < args.argc; i++) {
          os << args[i];
        }
      }
      break;
    }
  }
  return os;
}


// static
std::vector<const char*>* FlagList::argv() {
  std::vector<const char*>* args = new std::vector<const char*>(8);
  Flag* args_flag = nullptr;
  for (size_t i = 0; i < num_flags; ++i) {
    Flag* f = &flags[i];
    if (!f->IsDefault()) {
      if (f->type() == Flag::TYPE_ARGS) {
        DCHECK_NULL(args_flag);
        args_flag = f;  // Must be last in arguments.
        continue;
      }
      {
        bool disabled = f->type() == Flag::TYPE_BOOL && !*f->bool_variable();
        std::ostringstream os;
        os << (disabled ? "--no" : "--") << f->name();
        args->push_back(StrDup(os.str().c_str()));
      }
      if (f->type() != Flag::TYPE_BOOL) {
        std::ostringstream os;
        os << *f;
        args->push_back(StrDup(os.str().c_str()));
      }
    }
  }
  if (args_flag != nullptr) {
    std::ostringstream os;
    os << "--" << args_flag->name();
    args->push_back(StrDup(os.str().c_str()));
    JSArguments jsargs = *args_flag->args_variable();
    for (int j = 0; j < jsargs.argc; j++) {
      args->push_back(StrDup(jsargs[j]));
    }
  }
  return args;
}


inline char NormalizeChar(char ch) {
  return ch == '_' ? '-' : ch;
}

// Helper function to parse flags: Takes an argument arg and splits it into
// a flag name and flag value (or nullptr if they are missing). is_bool is set
// if the arg started with "-no" or "--no". The buffer may be used to NUL-
// terminate the name, it must be large enough to hold any possible name.
static void SplitArgument(const char* arg,
                          char* buffer,
                          int buffer_size,
                          const char** name,
                          const char** value,
                          bool* is_bool) {
  *name = nullptr;
  *value = nullptr;
  *is_bool = false;

  if (arg != nullptr && *arg == '-') {
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
      if (NormalizeChar(arg[0]) == '-') arg++;  // remove dash after "no".
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
      MemCopy(buffer, *name, n);
      buffer[n] = '\0';
      *name = buffer;
      // get the value
      *value = arg + 1;
    }
  }
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
  return nullptr;
}


// static
int FlagList::SetFlagsFromCommandLine(int* argc,
                                      char** argv,
                                      bool remove_flags) {
  int return_code = 0;
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

    if (name != nullptr) {
      // lookup the flag
      Flag* flag = FindFlag(name);
      if (flag == nullptr) {
        if (remove_flags) {
          // We don't recognize this flag but since we're removing
          // the flags we recognize we assume that the remaining flags
          // will be processed somewhere else so this flag might make
          // sense there.
          continue;
        } else {
          PrintF(stderr, "Error: unrecognized flag %s\n"
                 "Try --help for options\n", arg);
          return_code = j;
          break;
        }
      }

      // if we still need a flag value, use the next argument if available
      if (flag->type() != Flag::TYPE_BOOL &&
          flag->type() != Flag::TYPE_MAYBE_BOOL &&
          flag->type() != Flag::TYPE_ARGS && value == nullptr) {
        if (i < *argc) {
          value = argv[i++];
        }
        if (!value) {
          PrintF(stderr, "Error: missing value for flag %s of type %s\n"
                 "Try --help for options\n",
                 arg, Type2String(flag->type()));
          return_code = j;
          break;
        }
      }

      // set the flag
      char* endp = const_cast<char*>("");  // *endp is only read
      switch (flag->type()) {
        case Flag::TYPE_BOOL:
          *flag->bool_variable() = !is_bool;
          break;
        case Flag::TYPE_MAYBE_BOOL:
          *flag->maybe_bool_variable() = MaybeBoolFlag::Create(true, !is_bool);
          break;
        case Flag::TYPE_INT:
          *flag->int_variable() = static_cast<int>(strtol(value, &endp, 10));
          break;
        case Flag::TYPE_UINT: {
          // We do not use strtoul because it accepts negative numbers.
          int64_t val = static_cast<int64_t>(strtoll(value, &endp, 10));
          if (val < 0 || val > std::numeric_limits<unsigned int>::max()) {
            PrintF(stderr,
                   "Error: Value for flag %s of type %s is out of bounds "
                   "[0-%" PRIu64
                   "]\n"
                   "Try --help for options\n",
                   arg, Type2String(flag->type()),
                   static_cast<uint64_t>(
                       std::numeric_limits<unsigned int>::max()));
            return_code = j;
            break;
          }
          *flag->uint_variable() = static_cast<unsigned int>(val);
          break;
        }
        case Flag::TYPE_FLOAT:
          *flag->float_variable() = strtod(value, &endp);
          break;
        case Flag::TYPE_STRING:
          flag->set_string_value(value ? StrDup(value) : nullptr, true);
          break;
        case Flag::TYPE_ARGS: {
          int start_pos = (value == nullptr) ? i : i - 1;
          int js_argc = *argc - start_pos;
          const char** js_argv = NewArray<const char*>(js_argc);
          if (value != nullptr) {
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
      bool is_bool_type = flag->type() == Flag::TYPE_BOOL ||
          flag->type() == Flag::TYPE_MAYBE_BOOL;
      if ((is_bool_type && value != nullptr) || (!is_bool_type && is_bool) ||
          *endp != '\0') {
        PrintF(stderr, "Error: illegal value for flag %s of type %s\n"
               "Try --help for options\n",
               arg, Type2String(flag->type()));
        if (is_bool_type) {
          PrintF(stderr,
                 "To set or unset a boolean flag, use --flag or --no-flag.\n");
        }
        return_code = j;
        break;
      }

      // remove the flag & value from the command
      if (remove_flags) {
        while (j < i) {
          argv[j++] = nullptr;
        }
      }
    }
  }

  // shrink the argument list
  if (remove_flags) {
    int j = 1;
    for (int i = 1; i < *argc; i++) {
      if (argv[i] != nullptr) argv[j++] = argv[i];
    }
    *argc = j;
  }

  if (FLAG_help) {
    PrintHelp();
    exit(0);
  }
  // parsed all flags successfully
  return return_code;
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
  MemCopy(copy0.start(), str, len);
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
  CpuFeatures::Probe(false);
  CpuFeatures::PrintTarget();
  CpuFeatures::PrintFeatures();

  OFStream os(stdout);
  os << "Usage:\n"
        "  shell [options] -e string\n"
        "    execute string in V8\n"
        "  shell [options] file1 file2 ... filek\n"
        "    run JavaScript scripts in file1, file2, ..., filek\n"
        "  shell [options]\n"
        "  shell [options] --shell [file1 file2 ... filek]\n"
        "    run an interactive JavaScript shell\n"
        "  d8 [options] file1 file2 ... filek\n"
        "  d8 [options]\n"
        "  d8 [options] --shell [file1 file2 ... filek]\n"
        "    run the new debugging shell\n\n"
        "Options:\n";

  for (size_t i = 0; i < num_flags; ++i) {
    Flag* f = &flags[i];
    os << "  --" << f->name() << " (" << f->comment() << ")\n"
       << "        type: " << Type2String(f->type()) << "  default: " << *f
       << "\n";
  }
}


static uint32_t flag_hash = 0;


void ComputeFlagListHash() {
  std::ostringstream modified_args_as_string;
#ifdef DEBUG
  modified_args_as_string << "debug";
#endif  // DEBUG
  for (size_t i = 0; i < num_flags; ++i) {
    Flag* current = &flags[i];
    if (!current->IsDefault()) {
      modified_args_as_string << i;
      modified_args_as_string << *current;
    }
  }
  std::string args(modified_args_as_string.str());
  flag_hash = static_cast<uint32_t>(
      base::hash_range(args.c_str(), args.c_str() + args.length()));
}


// static
void FlagList::EnforceFlagImplications() {
#define FLAG_MODE_DEFINE_IMPLICATIONS
#include "src/flag-definitions.h"  // NOLINT(build/include)
#undef FLAG_MODE_DEFINE_IMPLICATIONS
  ComputeFlagListHash();
}


uint32_t FlagList::Hash() { return flag_hash; }
}  // namespace internal
}  // namespace v8
