// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags/flags.h"

#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdlib>
#include <sstream>

#include "src/base/functional.h"
#include "src/base/platform/platform.h"
#include "src/codegen/cpu-features.h"
#include "src/logging/counters.h"
#include "src/utils/allocation.h"
#include "src/utils/memcopy.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"
#include "src/wasm/wasm-limits.h"

namespace v8 {
namespace internal {

// Define all of our flags.
#define FLAG_MODE_DEFINE
#include "src/flags/flag-definitions.h"  // NOLINT(build/include)

// Define all of our flags default values.
#define FLAG_MODE_DEFINE_DEFAULTS
#include "src/flags/flag-definitions.h"  // NOLINT(build/include)

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
    TYPE_UINT64,
    TYPE_FLOAT,
    TYPE_SIZE_T,
    TYPE_STRING,
  };

  FlagType type_;       // What type of flag, bool, int, or string.
  const char* name_;    // Name of the flag, ex "my_flag".
  void* valptr_;        // Pointer to the global flag variable.
  const void* defptr_;  // Pointer to the default value.
  const char* cmt_;     // A comment about the flags purpose.
  bool owns_ptr_;       // Does the flag own its string value?

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

  uint64_t* uint64_variable() const {
    DCHECK(type_ == TYPE_UINT64);
    return reinterpret_cast<uint64_t*>(valptr_);
  }

  double* float_variable() const {
    DCHECK(type_ == TYPE_FLOAT);
    return reinterpret_cast<double*>(valptr_);
  }

  size_t* size_t_variable() const {
    DCHECK(type_ == TYPE_SIZE_T);
    return reinterpret_cast<size_t*>(valptr_);
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

  uint64_t uint64_default() const {
    DCHECK(type_ == TYPE_UINT64);
    return *reinterpret_cast<const uint64_t*>(defptr_);
  }

  double float_default() const {
    DCHECK(type_ == TYPE_FLOAT);
    return *reinterpret_cast<const double*>(defptr_);
  }

  size_t size_t_default() const {
    DCHECK(type_ == TYPE_SIZE_T);
    return *reinterpret_cast<const size_t*>(defptr_);
  }

  const char* string_default() const {
    DCHECK(type_ == TYPE_STRING);
    return *reinterpret_cast<const char* const*>(defptr_);
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
      case TYPE_UINT64:
        return *uint64_variable() == uint64_default();
      case TYPE_FLOAT:
        return *float_variable() == float_default();
      case TYPE_SIZE_T:
        return *size_t_variable() == size_t_default();
      case TYPE_STRING: {
        const char* str1 = string_value();
        const char* str2 = string_default();
        if (str2 == nullptr) return str1 == nullptr;
        if (str1 == nullptr) return str2 == nullptr;
        return strcmp(str1, str2) == 0;
      }
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
      case TYPE_UINT64:
        *uint64_variable() = uint64_default();
        break;
      case TYPE_FLOAT:
        *float_variable() = float_default();
        break;
      case TYPE_SIZE_T:
        *size_t_variable() = size_t_default();
        break;
      case TYPE_STRING:
        set_string_value(string_default(), false);
        break;
    }
  }
};

Flag flags[] = {
#define FLAG_MODE_META
#include "src/flags/flag-definitions.h"  // NOLINT(build/include)
};

const size_t num_flags = sizeof(flags) / sizeof(*flags);

}  // namespace

static const char* Type2String(Flag::FlagType type) {
  switch (type) {
    case Flag::TYPE_BOOL:
      return "bool";
    case Flag::TYPE_MAYBE_BOOL:
      return "maybe_bool";
    case Flag::TYPE_INT:
      return "int";
    case Flag::TYPE_UINT:
      return "uint";
    case Flag::TYPE_UINT64:
      return "uint64";
    case Flag::TYPE_FLOAT:
      return "float";
    case Flag::TYPE_SIZE_T:
      return "size_t";
    case Flag::TYPE_STRING:
      return "string";
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
    case Flag::TYPE_UINT64:
      os << *flag.uint64_variable();
      break;
    case Flag::TYPE_FLOAT:
      os << *flag.float_variable();
      break;
    case Flag::TYPE_SIZE_T:
      os << *flag.size_t_variable();
      break;
    case Flag::TYPE_STRING: {
      const char* str = flag.string_value();
      os << (str ? str : "nullptr");
      break;
    }
  }
  return os;
}

// static
std::vector<const char*>* FlagList::argv() {
  std::vector<const char*>* args = new std::vector<const char*>(8);
  for (size_t i = 0; i < num_flags; ++i) {
    Flag* f = &flags[i];
    if (!f->IsDefault()) {
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
  return args;
}

inline char NormalizeChar(char ch) { return ch == '_' ? '-' : ch; }

// Helper function to parse flags: Takes an argument arg and splits it into
// a flag name and flag value (or nullptr if they are missing). negated is set
// if the arg started with "-no" or "--no". The buffer may be used to NUL-
// terminate the name, it must be large enough to hold any possible name.
static void SplitArgument(const char* arg, char* buffer, int buffer_size,
                          const char** name, const char** value,
                          bool* negated) {
  *name = nullptr;
  *value = nullptr;
  *negated = false;

  if (arg != nullptr && *arg == '-') {
    // find the begin of the flag name
    arg++;  // remove 1st '-'
    if (*arg == '-') {
      arg++;                    // remove 2nd '-'
      DCHECK_NE('\0', arg[0]);  // '--' arguments are handled in the caller.
    }
    if (arg[0] == 'n' && arg[1] == 'o') {
      arg += 2;                                 // remove "no"
      if (NormalizeChar(arg[0]) == '-') arg++;  // remove dash after "no".
      *negated = true;
    }
    *name = arg;

    // find the end of the flag name
    while (*arg != '\0' && *arg != '=') arg++;

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
    if (EqualNames(name, flags[i].name())) return &flags[i];
  }
  return nullptr;
}

template <typename T>
bool TryParseUnsigned(Flag* flag, const char* arg, const char* value,
                      char** endp, T* out_val) {
  // We do not use strtoul because it accepts negative numbers.
  // Rejects values >= 2**63 when T is 64 bits wide but that
  // seems like an acceptable trade-off.
  uint64_t max = static_cast<uint64_t>(std::numeric_limits<T>::max());
  errno = 0;
  int64_t val = static_cast<int64_t>(strtoll(value, endp, 10));
  if (val < 0 || static_cast<uint64_t>(val) > max || errno != 0) {
    PrintF(stderr,
           "Error: Value for flag %s of type %s is out of bounds "
           "[0-%" PRIu64 "]\n",
           arg, Type2String(flag->type()), max);
    return false;
  }
  *out_val = static_cast<T>(val);
  return true;
}

// static
int FlagList::SetFlagsFromCommandLine(int* argc, char** argv,
                                      bool remove_flags) {
  int return_code = 0;
  // parse arguments
  for (int i = 1; i < *argc;) {
    int j = i;  // j > 0
    const char* arg = argv[i++];

    // split arg into flag components
    char buffer[1 * KB];
    const char* name;
    const char* value;
    bool negated;
    SplitArgument(arg, buffer, sizeof buffer, &name, &value, &negated);

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
          PrintF(stderr, "Error: unrecognized flag %s\n", arg);
          return_code = j;
          break;
        }
      }

      // if we still need a flag value, use the next argument if available
      if (flag->type() != Flag::TYPE_BOOL &&
          flag->type() != Flag::TYPE_MAYBE_BOOL && value == nullptr) {
        if (i < *argc) {
          value = argv[i++];
        }
        if (!value) {
          PrintF(stderr, "Error: missing value for flag %s of type %s\n", arg,
                 Type2String(flag->type()));
          return_code = j;
          break;
        }
      }

      // set the flag
      char* endp = const_cast<char*>("");  // *endp is only read
      switch (flag->type()) {
        case Flag::TYPE_BOOL:
          *flag->bool_variable() = !negated;
          break;
        case Flag::TYPE_MAYBE_BOOL:
          *flag->maybe_bool_variable() = MaybeBoolFlag::Create(true, !negated);
          break;
        case Flag::TYPE_INT:
          *flag->int_variable() = static_cast<int>(strtol(value, &endp, 10));
          break;
        case Flag::TYPE_UINT:
          if (!TryParseUnsigned(flag, arg, value, &endp,
                                flag->uint_variable())) {
            return_code = j;
          }
          break;
        case Flag::TYPE_UINT64:
          if (!TryParseUnsigned(flag, arg, value, &endp,
                                flag->uint64_variable())) {
            return_code = j;
          }
          break;
        case Flag::TYPE_FLOAT:
          *flag->float_variable() = strtod(value, &endp);
          break;
        case Flag::TYPE_SIZE_T:
          if (!TryParseUnsigned(flag, arg, value, &endp,
                                flag->size_t_variable())) {
            return_code = j;
          }
          break;
        case Flag::TYPE_STRING:
          flag->set_string_value(value ? StrDup(value) : nullptr, true);
          break;
      }

      // handle errors
      bool is_bool_type = flag->type() == Flag::TYPE_BOOL ||
                          flag->type() == Flag::TYPE_MAYBE_BOOL;
      if ((is_bool_type && value != nullptr) || (!is_bool_type && negated) ||
          *endp != '\0') {
        // TODO(neis): TryParseUnsigned may return with {*endp == '\0'} even in
        // an error case.
        PrintF(stderr, "Error: illegal value for flag %s of type %s\n", arg,
               Type2String(flag->type()));
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

  if (FLAG_help) {
    PrintHelp();
    exit(0);
  }

  if (remove_flags) {
    // shrink the argument list
    int j = 1;
    for (int i = 1; i < *argc; i++) {
      if (argv[i] != nullptr) argv[j++] = argv[i];
    }
    *argc = j;
  } else if (return_code != 0) {
    if (return_code + 1 < *argc) {
      PrintF(stderr, "The remaining arguments were ignored:");
      for (int i = return_code + 1; i < *argc; ++i) {
        PrintF(stderr, " %s", argv[i]);
      }
      PrintF(stderr, "\n");
    }
  }
  if (return_code != 0) PrintF(stderr, "Try --help for options\n");

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
int FlagList::SetFlagsFromString(const char* str, size_t len) {
  // make a 0-terminated copy of str
  std::unique_ptr<char[]> copy0{NewArray<char>(len + 1)};
  MemCopy(copy0.get(), str, len);
  copy0[len] = '\0';

  // strip leading white space
  char* copy = SkipWhiteSpace(copy0.get());

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

  return SetFlagsFromCommandLine(&argc, argv.begin(), false);
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

  StdoutStream os;
  os << "Synopsis:\n"
        "  shell [options] [--shell] [<file>...]\n"
        "  d8 [options] [-e <string>] [--shell] [[--module] <file>...]\n\n"
        "  -e        execute a string in V8\n"
        "  --shell   run an interactive JavaScript shell\n"
        "  --module  execute a file as a JavaScript module\n\n"
        "Note: the --module option is implicitly enabled for *.mjs files.\n\n"
        "The following syntax for options is accepted (both '-' and '--' are "
        "ok):\n"
        "  --flag        (bool flags only)\n"
        "  --no-flag     (bool flags only)\n"
        "  --flag=value  (non-bool flags only, no spaces around '=')\n"
        "  --flag value  (non-bool flags only)\n"
        "  --            (captures all remaining args in JavaScript)\n\n"
        "Options:\n";

  for (const Flag& f : flags) {
    os << "  --";
    for (const char* c = f.name(); *c != '\0'; ++c) {
      os << NormalizeChar(*c);
    }
    os << " (" << f.comment() << ")\n"
       << "        type: " << Type2String(f.type()) << "  default: " << f
       << "\n";
  }
}

static uint32_t flag_hash = 0;

void ComputeFlagListHash() {
  std::ostringstream modified_args_as_string;
  if (COMPRESS_POINTERS_BOOL) {
    modified_args_as_string << "ptr-compr";
  }
  if (DEBUG_BOOL) {
    modified_args_as_string << "debug";
  }
  for (size_t i = 0; i < num_flags; ++i) {
    Flag* current = &flags[i];
    if (current->type() == Flag::TYPE_BOOL &&
        current->bool_variable() == &FLAG_profile_deserialization) {
      // We want to be able to flip --profile-deserialization without
      // causing the code cache to get invalidated by this hash.
      continue;
    }
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
#include "src/flags/flag-definitions.h"  // NOLINT(build/include)
#undef FLAG_MODE_DEFINE_IMPLICATIONS
  ComputeFlagListHash();
}

uint32_t FlagList::Hash() { return flag_hash; }

#undef FLAG_MODE_DEFINE
#undef FLAG_MODE_DEFINE_DEFAULTS
#undef FLAG_MODE_META

}  // namespace internal
}  // namespace v8
