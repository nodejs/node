#ifndef SRC_NODE_OPTIONS_INL_H_
#define SRC_NODE_OPTIONS_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_options.h"
#include "util.h"
#include <cstdlib>

namespace node {

PerIsolateOptions* PerProcessOptions::get_per_isolate_options() {
  return per_isolate.get();
}

DebugOptions* EnvironmentOptions::get_debug_options() {
  return debug_options.get();
}

EnvironmentOptions* PerIsolateOptions::get_per_env_options() {
  return per_env.get();
}

template <typename Options>
void OptionsParser<Options>::AddOption(const std::string& name,
                                       bool Options::* field,
                                       OptionEnvvarSettings env_setting) {
  options_.emplace(name, OptionInfo {
    kBoolean,
    std::make_shared<SimpleOptionField<bool>>(field),
    env_setting
  });
}

template <typename Options>
void OptionsParser<Options>::AddOption(const std::string& name,
                                       int64_t Options::* field,
                                       OptionEnvvarSettings env_setting) {
  options_.emplace(name, OptionInfo {
    kInteger,
    std::make_shared<SimpleOptionField<int64_t>>(field),
    env_setting
  });
}

template <typename Options>
void OptionsParser<Options>::AddOption(const std::string& name,
                                       std::string Options::* field,
                                       OptionEnvvarSettings env_setting) {
  options_.emplace(name, OptionInfo {
    kString,
    std::make_shared<SimpleOptionField<std::string>>(field),
    env_setting
  });
}

template <typename Options>
void OptionsParser<Options>::AddOption(
    const std::string& name,
    std::vector<std::string> Options::* field,
    OptionEnvvarSettings env_setting) {
  options_.emplace(name, OptionInfo {
    kStringList,
    std::make_shared<SimpleOptionField<std::vector<std::string>>>(field),
    env_setting
  });
}

template <typename Options>
void OptionsParser<Options>::AddOption(const std::string& name,
                                       HostPort Options::* field,
                                       OptionEnvvarSettings env_setting) {
  options_.emplace(name, OptionInfo {
    kHostPort,
    std::make_shared<SimpleOptionField<HostPort>>(field),
    env_setting
  });
}

template <typename Options>
void OptionsParser<Options>::AddOption(const std::string& name, NoOp no_op_tag,
                                       OptionEnvvarSettings env_setting) {
  options_.emplace(name, OptionInfo { kNoOp, nullptr, env_setting });
}

template <typename Options>
void OptionsParser<Options>::AddOption(const std::string& name,
                                       V8Option v8_option_tag,
                                       OptionEnvvarSettings env_setting) {
  options_.emplace(name, OptionInfo { kV8Option, nullptr, env_setting });
}

template <typename Options>
void OptionsParser<Options>::AddAlias(const std::string& from,
                                      const std::string& to) {
  aliases_[from] = { to };
}

template <typename Options>
void OptionsParser<Options>::AddAlias(const std::string& from,
                                      const std::vector<std::string>& to) {
  aliases_[from] = to;
}

template <typename Options>
void OptionsParser<Options>::AddAlias(
    const std::string& from,
    const std::initializer_list<std::string>& to) {
  AddAlias(from, std::vector<std::string>(to));
}

template <typename Options>
void OptionsParser<Options>::Implies(const std::string& from,
                                     const std::string& to) {
  auto it = options_.find(to);
  CHECK_NE(it, options_.end());
  CHECK_EQ(it->second.type, kBoolean);
  implications_.emplace(from, Implication {
    std::static_pointer_cast<OptionField<bool>>(it->second.field), true
  });
}

template <typename Options>
void OptionsParser<Options>::ImpliesNot(const std::string& from,
                                        const std::string& to) {
  auto it = options_.find(to);
  CHECK_NE(it, options_.end());
  CHECK_EQ(it->second.type, kBoolean);
  implications_.emplace(from, Implication {
    std::static_pointer_cast<OptionField<bool>>(it->second.field), false
  });
}

template <typename Options>
template <typename OriginalField, typename ChildOptions>
auto OptionsParser<Options>::Convert(
    std::shared_ptr<OriginalField> original,
    ChildOptions* (Options::* get_child)()) {
  // If we have a field on ChildOptions, and we want to access it from an
  // Options instance, we call get_child() on the original Options and then
  // access it, i.e. this class implements a kind of function chaining.
  struct AdaptedField : BaseOptionField {
    void* LookupImpl(Options* options) const override {
      return original->LookupImpl((options->*get_child)());
    }

    AdaptedField(
        std::shared_ptr<OriginalField> original,
        ChildOptions* (Options::* get_child)())
          : original(original), get_child(get_child) {}

    std::shared_ptr<OriginalField> original;
    ChildOptions* (Options::* get_child)();
  };

  return std::shared_ptr<BaseOptionField>(
      new AdaptedField(original, get_child));
}
template <typename Options>
template <typename ChildOptions>
auto OptionsParser<Options>::Convert(
    typename OptionsParser<ChildOptions>::OptionInfo original,
    ChildOptions* (Options::* get_child)()) {
  return OptionInfo {
    original.type,
    Convert(original.field, get_child),
    original.env_setting
  };
}

template <typename Options>
template <typename ChildOptions>
auto OptionsParser<Options>::Convert(
    typename OptionsParser<ChildOptions>::Implication original,
    ChildOptions* (Options::* get_child)()) {
  return Implication {
    std::static_pointer_cast<OptionField<bool>>(
        Convert(original.target_field, get_child)),
    original.target_value
  };
}

template <typename Options>
template <typename ChildOptions>
void OptionsParser<Options>::Insert(
    OptionsParser<ChildOptions>* child_options_parser,
    ChildOptions* (Options::* get_child)()) {
  aliases_.insert(child_options_parser->aliases_.begin(),
                  child_options_parser->aliases_.end());

  for (const auto& pair : child_options_parser->options_)
    options_.emplace(pair.first, Convert(pair.second, get_child));

  for (const auto& pair : child_options_parser->implications_)
    implications_.emplace(pair.first, Convert(pair.second, get_child));
}

inline std::string NotAllowedInEnvErr(const std::string& arg) {
  return arg + " is not allowed in NODE_OPTIONS";
}

inline std::string RequiresArgumentErr(const std::string& arg) {
  return arg + " requires an argument";
}

// We store some of the basic information around a single Parse call inside
// this struct, to separate storage of command line arguments and their
// handling. In particular, this makes it easier to introduce 'synthetic'
// arguments that get inserted by expanding option aliases.
struct ArgsInfo {
  // Generally, the idea here is that the first entry in `*underlying` stores
  // the "0th" argument (the program name), then `synthetic_args` are inserted,
  // followed by the remainder of `*underlying`.
  std::vector<std::string>* underlying;
  std::vector<std::string> synthetic_args;

  std::vector<std::string>* exec_args;

  ArgsInfo(std::vector<std::string>* args,
           std::vector<std::string>* exec_args)
    : underlying(args), exec_args(exec_args) {}

  size_t remaining() const {
    // -1 to account for the program name.
    return underlying->size() - 1 + synthetic_args.size();
  }

  bool empty() const { return remaining() == 0; }
  const std::string& program_name() const { return underlying->at(0); }

  std::string& first() {
    return synthetic_args.empty() ? underlying->at(1) : synthetic_args.front();
  }

  std::string pop_first() {
    std::string ret = std::move(first());
    if (synthetic_args.empty()) {
      // Only push arguments to `exec_args` that were also originally passed
      // on the command line (i.e. not generated through alias expansion).
      // '--' is a special case here since its purpose is to end `exec_argv`,
      // which is why we do not include it.
      if (exec_args != nullptr && first() != "--")
        exec_args->push_back(ret);
      underlying->erase(underlying->begin() + 1);
    } else {
      synthetic_args.erase(synthetic_args.begin());
    }
    return ret;
  }
};

template <typename Options>
void OptionsParser<Options>::Parse(
    std::vector<std::string>* const orig_args,
    std::vector<std::string>* const exec_args,
    std::vector<std::string>* const v8_args,
    Options* const options,
    OptionEnvvarSettings required_env_settings,
    std::string* const error) {
  ArgsInfo args(orig_args, exec_args);

  // The first entry is the process name. Make sure it ends up in the V8 argv,
  // since V8::SetFlagsFromCommandLine() expects that to hold true for that
  // array as well.
  if (v8_args->empty())
    v8_args->push_back(args.program_name());

  while (!args.empty() && error->empty()) {
    if (args.first().size() <= 1 || args.first()[0] != '-') break;

    // We know that we're either going to consume this
    // argument or fail completely.
    const std::string arg = args.pop_first();

    if (arg == "--") {
      if (required_env_settings == kAllowedInEnvironment)
        *error = NotAllowedInEnvErr("--");
      break;
    }

    // Only allow --foo=bar notation for options starting with double dashes.
    // (E.g. -e=a is not allowed as shorthand for --eval=a, which would
    // otherwise be the result of alias expansion.)
    const std::string::size_type equals_index =
        arg[0] == '-' && arg[1] == '-' ? arg.find('=') : std::string::npos;
    std::string name =
      equals_index == std::string::npos ? arg : arg.substr(0, equals_index);

    // Store the 'original name' of the argument. This name differs from
    // 'name' in that it contains a possible '=' sign and is not affected
    // by alias expansion.
    std::string original_name = name;
    if (equals_index != std::string::npos)
      original_name += '=';

    {
      auto it = aliases_.end();
      // Expand aliases:
      // - If `name` can be found in `aliases_`.
      // - If `name` + '=' can be found in `aliases_`.
      // - If `name` + " <arg>" can be found in `aliases_`, and we have
      //   a subsequent argument that does not start with '-' itself.
      while ((it = aliases_.find(name)) != aliases_.end() ||
             (equals_index != std::string::npos &&
              (it = aliases_.find(name + '=')) != aliases_.end()) ||
             (!args.empty() &&
                 !args.first().empty() &&
                 args.first()[0] != '-' &&
              (it = aliases_.find(name + " <arg>")) != aliases_.end())) {
        const std::string prev_name = std::move(name);
        const std::vector<std::string>& expansion = it->second;

        // Use the first entry in the expansion as the new 'name'.
        name = expansion.front();

        if (expansion.size() > 1) {
          // The other arguments, if any, are going to be handled later.
          args.synthetic_args.insert(
              args.synthetic_args.begin(),
              expansion.begin() + 1,
              expansion.end());
        }

        if (name == prev_name) break;
      }
    }

    auto it = options_.find(name);

    if (it == options_.end()) {
      // We would assume that this is a V8 option if neither we nor any child
      // parser knows about it, so we convert - to _ for
      // canonicalization (since V8 accepts both) and look up again in order
      // to find a match.
      // TODO(addaleax): Make the canonicalization unconditional, i.e. allow
      // both - and _ in Node's own options as well.
      std::string::size_type index = 2;  // Start after initial '--'.
      while ((index = name.find('-', index + 1)) != std::string::npos)
        name[index] = '_';
      it = options_.find(name);
    }

    if ((it == options_.end() ||
         it->second.env_setting == kDisallowedInEnvironment) &&
        required_env_settings == kAllowedInEnvironment) {
      *error = NotAllowedInEnvErr(original_name);
      break;
    }

    if (it == options_.end()) {
      v8_args->push_back(arg);
      continue;
    }

    {
      auto implications = implications_.equal_range(name);
      for (auto it = implications.first; it != implications.second; ++it)
        *it->second.target_field->Lookup(options) = it->second.target_value;
    }

    const OptionInfo& info = it->second;
    std::string value;
    if (info.type != kBoolean && info.type != kNoOp && info.type != kV8Option) {
      if (equals_index != std::string::npos) {
        value = arg.substr(equals_index + 1);
        if (value.empty()) {
        missing_argument:
          *error = RequiresArgumentErr(original_name);
          break;
        }
      } else {
        if (args.empty())
          goto missing_argument;

        value = args.pop_first();

        if (!value.empty() && value[0] == '-') {
          goto missing_argument;
        } else {
          if (!value.empty() && value[0] == '\\' && value[1] == '-')
            value = value.substr(1);  // Treat \- as escaping an -.
        }
      }
    }

    switch (info.type) {
      case kBoolean:
        *std::static_pointer_cast<OptionField<bool>>(info.field)
            ->Lookup(options) = true;
        break;
      case kInteger:
        *std::static_pointer_cast<OptionField<int64_t>>(info.field)
            ->Lookup(options) = std::atoll(value.c_str());
        break;
      case kString:
        *std::static_pointer_cast<OptionField<std::string>>(info.field)
            ->Lookup(options) = value;
        break;
      case kStringList:
        std::static_pointer_cast<OptionField<std::vector<std::string>>>(
            info.field)->Lookup(options)->emplace_back(std::move(value));
        break;
      case kHostPort:
        std::static_pointer_cast<OptionField<HostPort>>(info.field)
            ->Lookup(options)->Update(SplitHostPort(value, error));
        break;
      case kNoOp:
        break;
      case kV8Option:
        v8_args->push_back(arg);
        break;
      default:
        UNREACHABLE();
    }
  }
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_OPTIONS_INL_H_
