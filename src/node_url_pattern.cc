#include "node_url_pattern.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_mem-inl.h"
#include "node_process.h"
#include "path.h"
#include "util-inl.h"

#include <iterator>
#include <span>
#include <unordered_set>

namespace node {
using node::url_pattern::URLPatternRegexProvider;

template <>
struct MemoryRetainerTraits<ada::url_pattern<URLPatternRegexProvider>> {
  using Type = ada::url_pattern<URLPatternRegexProvider>;
  static void MemoryInfo(MemoryTracker* tracker, const Type& value) {
    tracker->TraitTrackInline(value.protocol_component, "protocol_component");
    tracker->TraitTrackInline(value.username_component, "username_component");
    tracker->TraitTrackInline(value.password_component, "password_component");
    tracker->TraitTrackInline(value.hostname_component, "hostname_component");
    tracker->TraitTrackInline(value.port_component, "port_component");
    tracker->TraitTrackInline(value.pathname_component, "pathname_component");
    tracker->TraitTrackInline(value.search_component, "search_component");
    tracker->TraitTrackInline(value.hash_component, "hash_component");
  }

  static const char* MemoryInfoName(const Type& value) {
    return "ada::url_pattern";
  }

  static size_t SelfSize(const Type& value) { return sizeof(value); }
};

template <>
struct MemoryRetainerTraits<
    ada::url_pattern_component<URLPatternRegexProvider>> {
  using Type = ada::url_pattern_component<URLPatternRegexProvider>;
  static void MemoryInfo(MemoryTracker* tracker, const Type& value) {
    tracker->TrackField("pattern", value.pattern);
    tracker->TrackField("group_name_list", value.group_name_list);
  }

  static const char* MemoryInfoName(const Type& value) {
    return "ada::url_pattern_component";
  }

  static size_t SelfSize(const Type& value) { return sizeof(value); }
};

}  // namespace node

namespace node::url_pattern {

using v8::Array;
using v8::Context;
using v8::DictionaryTemplate;
using v8::DontDelete;
using v8::DontEnum;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::Name;
using v8::NewStringType;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::RegExp;
using v8::Signature;
using v8::String;
using v8::Symbol;
using v8::Value;

std::optional<URLPatternRegexProvider::regex_type>
URLPatternRegexProvider::create_instance(std::string_view pattern,
                                         bool ignore_case) {
  auto isolate = Isolate::GetCurrent();
  auto env = Environment::GetCurrent(isolate);
  int flags = RegExp::Flags::kUnicodeSets | RegExp::Flags::kDotAll;
  if (ignore_case) {
    flags |= static_cast<int>(RegExp::Flags::kIgnoreCase);
  }

  Local<String> local_pattern;
  if (!String::NewFromUtf8(
           isolate, pattern.data(), NewStringType::kNormal, pattern.size())
           .ToLocal(&local_pattern)) {
    return std::nullopt;
  }
  Local<RegExp> regexp;
  if (!RegExp::New(
           env->context(), local_pattern, static_cast<RegExp::Flags>(flags))
           .ToLocal(&regexp)) {
    return std::nullopt;
  }
  return Global<RegExp>(isolate, regexp);
}

bool URLPatternRegexProvider::regex_match(std::string_view input,
                                          const regex_type& pattern) {
  auto isolate = Isolate::GetCurrent();
  auto env = Environment::GetCurrent(isolate);
  Local<String> local_input;
  if (!String::NewFromUtf8(
           isolate, input.data(), NewStringType::kNormal, input.size())
           .ToLocal(&local_input)) {
    return false;
  }
  Local<Object> result_object;
  if (!pattern.Get(isolate)
           ->Exec(env->context(), local_input)
           .ToLocal(&result_object)) {
    return false;
  }
  // RegExp::Exec returns null if there is no match.
  return !result_object->IsNull();
}

std::optional<std::vector<std::optional<std::string>>>
URLPatternRegexProvider::regex_search(std::string_view input,
                                      const regex_type& global_pattern) {
  auto isolate = Isolate::GetCurrent();
  auto env = Environment::GetCurrent(isolate);
  Local<String> local_input;
  if (!String::NewFromUtf8(
           isolate, input.data(), NewStringType::kNormal, input.size())
           .ToLocal(&local_input)) {
    return std::nullopt;
  }
  Local<Object> exec_result_object;
  auto pattern = global_pattern.Get(isolate);
  if (!pattern->Exec(env->context(), local_input)
           .ToLocal(&exec_result_object) ||
      exec_result_object->IsNull()) {
    return std::nullopt;
  }
  DCHECK(exec_result_object->IsArray());
  auto exec_result = exec_result_object.As<Array>();
  size_t len = exec_result->Length();
  std::vector<std::optional<std::string>> result;
  result.reserve(len);
  for (size_t i = 1; i < len; i++) {
    Local<Value> entry;
    if (!exec_result->Get(env->context(), i).ToLocal(&entry)) {
      return std::nullopt;
    }

    // V8 checks that the regexp exec result is one of the correct types.
    DCHECK_IMPLIES(!entry->IsUndefined(), entry->IsString());

    if (entry->IsUndefined()) {
      result.emplace_back(std::nullopt);
    } else if (entry->IsString()) {
      Utf8Value utf8_entry(isolate, entry.As<String>());
      result.emplace_back(utf8_entry.ToString());
    }
  }
  return result;
}

URLPattern::URLPattern(Environment* env,
                       Local<Object> object,
                       ada::url_pattern<URLPatternRegexProvider>&& url_pattern)
    : BaseObject(env, object), url_pattern_(std::move(url_pattern)) {
  MakeWeak();
}

void URLPattern::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TraitTrackInline(url_pattern_, "url_pattern");
#define URL_PATTERN_CACHED_VALUES(_, lowercase)                                \
  tracker->TrackField(#lowercase, lowercase);
  URL_PATTERN_COMPONENTS(URL_PATTERN_CACHED_VALUES)
#undef URL_PATTERN_CACHED_VALUES
}

void URLPattern::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args.IsConstructCall()) {
    THROW_ERR_CONSTRUCT_CALL_REQUIRED(env);
    return;
  }

  // If no arguments are passed, then we parse the empty URLPattern.
  if (args.Length() == 0) {
    auto init = ada::url_pattern_init{};
    auto url_pattern = ada::parse_url_pattern<URLPatternRegexProvider>(init);
    CHECK(url_pattern);
    new URLPattern(env, args.This(), std::move(*url_pattern));
    return;
  }

  std::optional<ada::url_pattern_init> init{};
  std::optional<std::string> input{};
  std::optional<std::string> base_url{};
  std::optional<ada::url_pattern_options> options{};

  // Following patterns are supported:
  // - new URLPattern(input)
  // - new URLPattern(input, baseURL)
  // - new URLPattern(input, options)
  // - new URLPattern(input, baseURL, options)
  // Per WebIDL, null/undefined for a union type including a dictionary
  // uses the default value (empty init).
  if (args[0]->IsNullOrUndefined()) {
    init = ada::url_pattern_init{};
  } else if (args[0]->IsString()) {
    BufferValue input_buffer(env->isolate(), args[0]);
    CHECK_NOT_NULL(*input_buffer);
    input = input_buffer.ToString();
  } else if (args[0]->IsObject()) {
    init = URLPatternInit::FromJsObject(env, args[0].As<Object>());
    // If init does not have a value here, the implication is that an
    // error was thrown. Let's allow that to be handled now by returning
    // early. If we don't, the error thrown will be swallowed.
    if (!init) return;
  } else {
    THROW_ERR_INVALID_ARG_TYPE(env, "Input must be an object or a string");
    return;
  }

  // Per WebIDL overload resolution:
  // With 3+ args, it's always overload 1: (input, baseURL, options)
  // With 2 args, if arg1 is string, it is overload 1 (baseURL),
  //   otherwise overload 2 (options)
  if (args.Length() >= 3) {
    // arg1 is baseURL. Per WebIDL, null/undefined are stringified for
    // USVString ("null"/"undefined"), which will be rejected as invalid
    // URLs by ada downstream.
    if (args[1]->IsString()) {
      BufferValue base_url_buffer(env->isolate(), args[1]);
      CHECK_NOT_NULL(*base_url_buffer);
      base_url = base_url_buffer.ToString();
    } else if (args[1]->IsNull()) {
      base_url = std::string("null");
    } else if (args[1]->IsUndefined()) {
      base_url = std::string("undefined");
    } else {
      THROW_ERR_INVALID_ARG_TYPE(env, "second argument must be a string");
      return;
    }

    // arg2 is options. Per WebIDL, null/undefined for a dictionary
    // uses the default value (empty dict).
    if (!args[2]->IsNullOrUndefined()) {
      if (!args[2]->IsObject()) {
        THROW_ERR_INVALID_ARG_TYPE(env, "options must be an object");
        return;
      }
      CHECK(!options.has_value());
      options = URLPatternOptions::FromJsObject(env, args[2].As<Object>());
      if (!options) return;
    }
  } else if (args.Length() == 2) {
    // Overload resolution: string is overload 1 (baseURL),
    // otherwise overload 2 (options).
    if (args[1]->IsString()) {
      BufferValue base_url_buffer(env->isolate(), args[1]);
      CHECK_NOT_NULL(*base_url_buffer);
      base_url = base_url_buffer.ToString();
    } else if (args[1]->IsNullOrUndefined()) {
      // Overload 2, options uses default.
    } else if (args[1]->IsObject()) {
      CHECK(!options.has_value());
      options = URLPatternOptions::FromJsObject(env, args[1].As<Object>());
      if (!options) return;
    } else {
      THROW_ERR_INVALID_ARG_TYPE(env,
                                 "second argument must be a string or object");
      return;
    }
  }

  // Either url_pattern_init or input as a string must be provided.
  CHECK_IMPLIES(init.has_value(), !input.has_value());

  std::string_view base_url_view{};
  if (base_url) base_url_view = {base_url->data(), base_url->size()};

  ada::url_pattern_input arg0;
  if (init.has_value()) {
    arg0 = std::move(*init);
  } else {
    arg0 = std::move(*input);
  }

  auto url_pattern = parse_url_pattern<URLPatternRegexProvider>(
      std::move(arg0),
      base_url ? &base_url_view : nullptr,
      options.has_value() ? &options.value() : nullptr);

  if (!url_pattern) {
    THROW_ERR_INVALID_URL_PATTERN(env, "Failed to construct URLPattern");
    return;
  }

  new URLPattern(env, args.This(), std::move(*url_pattern));
}

MaybeLocal<Value> URLPattern::URLPatternInit::ToJsObject(
    Environment* env, const ada::url_pattern_init& init) {
  auto isolate = env->isolate();
  auto context = env->context();

  auto tmpl = env->urlpatterninit_template();
  if (tmpl.IsEmpty()) {
    static constexpr std::string_view namesVec[] = {
        "baseURL",
        "hash",
        "hostname",
        "password",
        "pathname",
        "port",
        "protocol",
        "search",
        "username",
    };
    tmpl = DictionaryTemplate::New(isolate, namesVec);
    env->set_urlpatterninit_template(tmpl);
  }

  MaybeLocal<Value> values[9];
  int idx = 0;
  Local<Value> temp;
  const auto trySet = [&](const std::optional<std::string>& val) {
    if (val.has_value()) {
      if (!ToV8Value(context, *val).ToLocal(&temp)) {
        return false;
      }
      values[idx] = temp;
    }
    idx++;
    return true;
  };

  if (!trySet(init.base_url) || !trySet(init.hash) || !trySet(init.hostname) ||
      !trySet(init.password) || !trySet(init.pathname) || !trySet(init.port) ||
      !trySet(init.protocol) || !trySet(init.search) ||
      !trySet(init.username)) {
    return {};
  }
  return tmpl->NewInstance(context, values);
}

std::optional<ada::url_pattern_init> URLPattern::URLPatternInit::FromJsObject(
    Environment* env, Local<Object> obj) {
  ada::url_pattern_init init{};
  Local<String> components[] = {
      env->base_url_string(),
      env->hash_string(),
      env->hostname_string(),
      env->password_string(),
      env->pathname_string(),
      env->port_string(),
      env->protocol_string(),
      env->search_string(),
      env->username_string(),
  };
  auto isolate = env->isolate();
  const auto set_parameter = [&](std::string_view key, std::string_view value) {
    if (key == "protocol") {
      init.protocol = std::string(value);
    } else if (key == "username") {
      init.username = std::string(value);
    } else if (key == "password") {
      init.password = std::string(value);
    } else if (key == "hostname") {
      init.hostname = std::string(value);
    } else if (key == "port") {
      init.port = std::string(value);
    } else if (key == "pathname") {
      init.pathname = std::string(value);
    } else if (key == "search") {
      init.search = std::string(value);
    } else if (key == "hash") {
      init.hash = std::string(value);
    } else if (key == "baseURL") {
      init.base_url = std::string(value);
    }
  };
  Local<Value> value;
  for (const auto& component : components) {
    Utf8Value key(isolate, component);
    if (obj->Get(env->context(), component).ToLocal(&value)) {
      if (value->IsString()) {
        Utf8Value utf8_value(isolate, value);
        set_parameter(key.ToStringView(), utf8_value.ToStringView());
      }
    } else {
      // If ToLocal failed then we assume an error occurred,
      // bail out early to propagate the error.
      return std::nullopt;
    }
  }
  return init;
}

MaybeLocal<Object> URLPattern::URLPatternComponentResult::ToJSObject(
    Environment* env, const ada::url_pattern_component_result& result) {
  auto isolate = env->isolate();
  auto context = env->context();
  LocalVector<Name> group_names(isolate);
  LocalVector<Value> group_values(isolate);
  group_names.reserve(result.groups.size());
  group_values.reserve(result.groups.size());
  for (const auto& [group_key, group_value] : result.groups) {
    Local<Value> key;
    if (!ToV8Value(context, group_key).ToLocal(&key)) {
      return {};
    }
    group_names.push_back(key.As<Name>());
    Local<Value> value;
    if (group_value) {
      if (!ToV8Value(env->context(), *group_value).ToLocal(&value)) {
        return {};
      }
    } else {
      value = Undefined(isolate);
    }
    group_values.push_back(value);
  }
  auto parsed_group = Object::New(isolate,
                                  Object::New(isolate),
                                  group_names.data(),
                                  group_values.data(),
                                  group_names.size());

  Local<Value> input;
  if (!ToV8Value(env->context(), result.input).ToLocal(&input)) {
    return {};
  }

  auto tmpl = env->urlpatterncomponentresult_template();
  if (tmpl.IsEmpty()) {
    static constexpr std::string_view namesVec[] = {
        "groups",
        "input",
    };
    tmpl = DictionaryTemplate::New(isolate, namesVec);
    env->set_urlpatterncomponentresult_template(tmpl);
  }
  MaybeLocal<Value> values[] = {parsed_group, input};
  return NewDictionaryInstance(env->context(), tmpl, values);
}

MaybeLocal<Value> URLPattern::URLPatternResult::ToJSValue(
    Environment* env, const ada::url_pattern_result& result) {
  return ToJSValue(env, result, Local<Object>());
}

MaybeLocal<Value> URLPattern::URLPatternResult::ToJSValue(
    Environment* env,
    const ada::url_pattern_result& result,
    Local<Object> pattern) {
  auto isolate = env->isolate();

  // Per WebIDL, the members of a dictionary are ordered lexicographically,
  // after the members of the dictionary it inherits from. The `pattern` member
  // of a URLPatternListResult therefore comes after the URLPatternResult ones.
  static constexpr std::string_view namesVec[] = {
      "hash",
      "hostname",
      "inputs",
      "password",
      "pathname",
      "port",
      "protocol",
      "search",
      "username",
      "pattern",
  };
  constexpr size_t kResultMemberCount = std::size(namesVec) - 1;

  Local<DictionaryTemplate> tmpl;
  if (pattern.IsEmpty()) {
    tmpl = env->urlpatternresult_template();
    if (tmpl.IsEmpty()) {
      tmpl = DictionaryTemplate::New(
          isolate,
          std::span<const std::string_view>(namesVec).first(
              kResultMemberCount));
      env->set_urlpatternresult_template(tmpl);
    }
  } else {
    tmpl = env->urlpatternlistresult_template();
    if (tmpl.IsEmpty()) {
      tmpl = DictionaryTemplate::New(isolate, namesVec);
      env->set_urlpatternlistresult_template(tmpl);
    }
  }

  size_t index = 0;
  MaybeLocal<Value> vals[] = {
      URLPatternComponentResult::ToJSObject(env, result.hash),
      URLPatternComponentResult::ToJSObject(env, result.hostname),
      Array::New(env->context(),
                 result.inputs.size(),
                 [&index, &inputs = result.inputs, env]() {
                   auto& input = inputs[index++];
                   if (std::holds_alternative<std::string_view>(input)) {
                     auto input_str = std::get<std::string_view>(input);
                     return ToV8Value(env->context(), input_str);
                   } else {
                     DCHECK(
                         std::holds_alternative<ada::url_pattern_init>(input));
                     auto init = std::get<ada::url_pattern_init>(input);
                     return URLPatternInit::ToJsObject(env, init);
                   }
                 }),
      URLPatternComponentResult::ToJSObject(env, result.password),
      URLPatternComponentResult::ToJSObject(env, result.pathname),
      URLPatternComponentResult::ToJSObject(env, result.port),
      URLPatternComponentResult::ToJSObject(env, result.protocol),
      URLPatternComponentResult::ToJSObject(env, result.search),
      URLPatternComponentResult::ToJSObject(env, result.username),
      pattern};
  std::span<MaybeLocal<Value>> values(vals);
  return NewDictionaryInstanceNullProto(
      env->context(),
      tmpl,
      pattern.IsEmpty() ? values.first(kResultMemberCount) : values);
}

std::optional<ada::url_pattern_options>
URLPattern::URLPatternOptions::FromJsObject(Environment* env,
                                            Local<Object> obj) {
  ada::url_pattern_options options{};
  Local<Value> ignore_case;
  if (obj->Get(env->context(), env->ignore_case_string())
          .ToLocal(&ignore_case)) {
    // Per WebIDL, boolean dictionary members are coerced (not type-checked).
    options.ignore_case = ignore_case->BooleanValue(env->isolate());
  } else {
    // If ToLocal returns false, the assumption is that getting the
    // ignore_case_string threw an error, let's propagate that now
    // by returning std::nullopt.
    return std::nullopt;
  }

  return options;
}

// Perform value lookup and cache the result in a v8::Global.
#define URL_PATTERN_COMPONENT_GETTERS(uppercase_name, lowercase_name)          \
  MaybeLocal<Value> URLPattern::uppercase_name() {                             \
    auto isolate = env()->isolate();                                           \
    if (lowercase_name.IsEmpty()) {                                            \
      Local<Value> value;                                                      \
      if (ToV8Value(env()->context(), url_pattern_.get_##lowercase_name())     \
              .ToLocal(&value)) {                                              \
        lowercase_name.Reset(isolate, value);                                  \
        return value;                                                          \
      }                                                                        \
      return {};                                                               \
    }                                                                          \
    return lowercase_name.Get(isolate);                                        \
  }
URL_PATTERN_COMPONENTS(URL_PATTERN_COMPONENT_GETTERS)
#undef URL_PATTERN_COMPONENT_GETTERS

bool URLPattern::HasRegExpGroups() const {
  return url_pattern_.has_regexp_groups();
}

// Instance methods

MaybeLocal<Value> URLPattern::Exec(Environment* env,
                                   const ada::url_pattern_input& input,
                                   std::optional<std::string_view>& baseURL) {
  if (auto result = url_pattern_.exec(input, baseURL ? &*baseURL : nullptr)) {
    if (result->has_value()) {
      return URLPatternResult::ToJSValue(env, result->value());
    }
    return Null(env->isolate());
  }
  return {};
}

bool URLPattern::Test(Environment* env,
                      const ada::url_pattern_input& input,
                      std::optional<std::string_view>& baseURL) {
  if (auto result = url_pattern_.test(input, baseURL ? &*baseURL : nullptr)) {
    return *result;
  }
  THROW_ERR_OPERATION_FAILED(env, "Failed to test URLPattern");
  return false;
}

// V8 Methods

namespace {
// The arguments of URLPattern.prototype.exec() and test(). When the input is
// a string, `input` refers to `input_storage`, so instances must not be copied
// or moved.
struct URLPatternMatchArgs {
  URLPatternMatchArgs() = default;
  URLPatternMatchArgs(const URLPatternMatchArgs&) = delete;
  URLPatternMatchArgs& operator=(const URLPatternMatchArgs&) = delete;

  std::optional<std::string_view> base_url_view() const {
    return base_url ? std::optional<std::string_view>(*base_url) : std::nullopt;
  }

  ada::url_pattern_input input;
  std::string input_storage;
  std::optional<std::string> base_url;
};

// Parses the `input` and `baseURL` arguments of exec() and test(). Returns
// false if a JavaScript exception is pending.
bool ParseURLPatternMatchArgs(Environment* env,
                              const FunctionCallbackInfo<Value>& args,
                              URLPatternMatchArgs* out) {
  if (args.Length() == 0 || args[0]->IsNullOrUndefined()) {
    out->input = ada::url_pattern_init{};
  } else if (args[0]->IsString()) {
    Utf8Value input_value(env->isolate(), args[0].As<String>());
    out->input_storage = input_value.ToString();
    out->input = std::string_view(out->input_storage);
  } else if (args[0]->IsObject()) {
    auto maybeInput =
        URLPattern::URLPatternInit::FromJsObject(env, args[0].As<Object>());
    if (!maybeInput.has_value()) return false;
    out->input = std::move(*maybeInput);
  } else {
    THROW_ERR_INVALID_ARG_TYPE(
        env, "URLPattern input needs to be a string or an object");
    return false;
  }

  if (args.Length() > 1 && !args[1]->IsUndefined()) {
    if (args[1]->IsNull()) {
      out->base_url = std::string("null");
    } else if (args[1]->IsString()) {
      Utf8Value base_url_value(env->isolate(), args[1].As<String>());
      out->base_url = base_url_value.ToStringView();
    } else {
      THROW_ERR_INVALID_ARG_TYPE(env, "baseURL must be a string");
      return false;
    }
  }
  return true;
}
}  // namespace

void URLPattern::Exec(const FunctionCallbackInfo<Value>& args) {
  URLPattern* url_pattern;
  ASSIGN_OR_RETURN_UNWRAP(&url_pattern, args.This());
  auto env = Environment::GetCurrent(args);

  URLPatternMatchArgs match_args;
  if (!ParseURLPatternMatchArgs(env, args, &match_args)) return;

  std::optional<std::string_view> base_url = match_args.base_url_view();
  Local<Value> result;
  if (!url_pattern->Exec(env, match_args.input, base_url).ToLocal(&result)) {
    THROW_ERR_OPERATION_FAILED(env, "Failed to exec URLPattern");
    return;
  }
  args.GetReturnValue().Set(result);
}

void URLPattern::Test(const FunctionCallbackInfo<Value>& args) {
  URLPattern* url_pattern;
  ASSIGN_OR_RETURN_UNWRAP(&url_pattern, args.This());
  auto env = Environment::GetCurrent(args);

  URLPatternMatchArgs match_args;
  if (!ParseURLPatternMatchArgs(env, args, &match_args)) return;

  std::optional<std::string_view> base_url = match_args.base_url_view();
  args.GetReturnValue().Set(url_pattern->Test(env, match_args.input, base_url));
}

#define URL_PATTERN_COMPONENT_GETTERS(uppercase_name, lowercase_name)          \
  void URLPattern::uppercase_name(const FunctionCallbackInfo<Value>& info) {   \
    URLPattern* url_pattern;                                                   \
    ASSIGN_OR_RETURN_UNWRAP(&url_pattern, info.This());                        \
    Local<Value> result;                                                       \
    if (url_pattern->uppercase_name().ToLocal(&result)) {                      \
      info.GetReturnValue().Set(result);                                       \
    }                                                                          \
  }
URL_PATTERN_COMPONENTS(URL_PATTERN_COMPONENT_GETTERS)
#undef URL_PATTERN_COMPONENT_GETTERS

void URLPattern::HasRegexpGroups(const FunctionCallbackInfo<Value>& info) {
  URLPattern* url_pattern;
  ASSIGN_OR_RETURN_UNWRAP(&url_pattern, info.This());
  info.GetReturnValue().Set(url_pattern->HasRegExpGroups());
}

namespace {
// Processes `input` and `base_url` as the URLPattern "match" algorithm does
// before it runs the component regular expressions, mirroring
// ada::url_pattern::match(). Returns errors::type_error where "match" throws,
// and std::nullopt where it returns null.
// https://urlpattern.spec.whatwg.org/#url-pattern-match
ada::result<std::optional<URLPatternList::Components>> ProcessMatchInput(
    const ada::url_pattern_input& input, const std::string_view* base_url) {
  URLPatternList::Components components;

  if (const auto* init = std::get_if<ada::url_pattern_init>(&input)) {
    if (base_url != nullptr) {
      return tl::unexpected(ada::errors::type_error);
    }
    auto processed =
        ada::url_pattern_init::process(*init,
                                       ada::url_pattern_init::process_type::url,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "",
                                       "",
                                       "",
                                       "");
    if (!processed) {
      return std::nullopt;
    }
    components.protocol = std::move(processed->protocol).value_or("");
    components.username = std::move(processed->username).value_or("");
    components.password = std::move(processed->password).value_or("");
    components.hostname = std::move(processed->hostname).value_or("");
    components.port = std::move(processed->port).value_or("");
    components.pathname = std::move(processed->pathname).value_or("");
    components.search = std::move(processed->search).value_or("");
    if (components.search.starts_with('?')) {
      components.search.erase(0, 1);
    }
    components.hash = std::move(processed->hash).value_or("");
    return components;
  }

  std::optional<ada::url_aggregator> base;
  if (base_url != nullptr) {
    auto parsed_base = ada::parse<ada::url_aggregator>(*base_url, nullptr);
    if (!parsed_base) {
      return std::nullopt;
    }
    base = std::move(*parsed_base);
  }
  auto url = ada::parse<ada::url_aggregator>(std::get<std::string_view>(input),
                                             base ? &*base : nullptr);
  if (!url) {
    return std::nullopt;
  }

  std::string_view protocol = url->get_protocol();
  protocol.remove_suffix(1);  // The trailing ':'.
  components.protocol = protocol;
  components.username = url->get_username();
  components.password = url->get_password();
  components.hostname = url->get_hostname();
  components.port = url->get_port();
  components.pathname = url->get_pathname();
  if (url->has_search()) {
    std::string_view search = url->get_search();
    if (search.starts_with('?')) search.remove_prefix(1);
    components.search = search;
  }
  if (url->has_hash()) {
    std::string_view hash = url->get_hash();
    if (hash.starts_with('#')) hash.remove_prefix(1);
    components.hash = hash;
  }
  return components;
}

// Converts `value` to a WebIDL sequence<URLPattern>. Per WebIDL, any iterable
// is read through its iterator, including arrays, and the conversion stops at
// the first value that is not a URLPattern. Returns false if a JavaScript
// exception is pending.
// https://webidl.spec.whatwg.org/#js-sequence
bool ToURLPatternSequence(Environment* env,
                          Local<Value> value,
                          std::vector<BaseObjectPtr<URLPattern>>* patterns) {
  static constexpr std::string_view kNotIterable =
      "The \"patterns\" argument must be an iterable of URLPattern objects";
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  if (!value->IsObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env, kNotIterable);
    return false;
  }
  Local<Object> iterable = value.As<Object>();
  Local<Value> method;
  if (!iterable->Get(context, Symbol::GetIterator(isolate)).ToLocal(&method)) {
    return false;
  }
  if (!method->IsFunction()) {
    THROW_ERR_INVALID_ARG_TYPE(env, kNotIterable);
    return false;
  }
  Local<Value> iterator;
  if (!method.As<Function>()
           ->Call(context, iterable, 0, nullptr)
           .ToLocal(&iterator)) {
    return false;
  }
  if (!iterator->IsObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env, kNotIterable);
    return false;
  }
  Local<Value> next;
  if (!iterator.As<Object>()->Get(context, env->next_string()).ToLocal(&next)) {
    return false;
  }
  if (!next->IsFunction()) {
    THROW_ERR_INVALID_ARG_TYPE(env, kNotIterable);
    return false;
  }

  while (true) {
    HandleScope handle_scope(isolate);
    Local<Value> result;
    if (!next.As<Function>()
             ->Call(context, iterator, 0, nullptr)
             .ToLocal(&result)) {
      return false;
    }
    if (!result->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env, kNotIterable);
      return false;
    }
    Local<Value> done;
    if (!result.As<Object>()->Get(context, env->done_string()).ToLocal(&done)) {
      return false;
    }
    if (done->BooleanValue(isolate)) {
      return true;
    }
    Local<Value> item;
    if (!result.As<Object>()
             ->Get(context, env->value_string())
             .ToLocal(&item)) {
      return false;
    }
    if (!URLPattern::HasInstance(env, item)) {
      THROW_ERR_INVALID_ARG_TYPE(
          env,
          "The \"patterns[%d]\" argument must be an instance of URLPattern",
          patterns->size());
      return false;
    }
    patterns->emplace_back(BaseObject::Unwrap<URLPattern>(item));
  }
}
}  // namespace

URLPatternList::URLPatternList(
    Environment* env,
    Local<Object> object,
    std::vector<BaseObjectPtr<URLPattern>>&& patterns)
    : BaseObject(env, object), patterns_(std::move(patterns)) {
  MakeWeak();
  BuildPathnameIndex();
}

void URLPatternList::BuildPathnameIndex() {
  // ada::url_pattern_list compiles every pathname as the pathname of a URL
  // with a special scheme, with one ignore_case flag for the whole index. A
  // pattern is only indexed if its own pathname component was compiled that
  // way, so that the index and the pattern agree on which pathnames match.
  std::optional<bool> ignore_case;
  std::vector<std::string_view> pathnames;
  for (size_t i = 0; i < patterns_.size(); i++) {
    auto& pattern = patterns_[i]->url_pattern_;
    bool indexable =
        ada::url_pattern_helpers::protocol_component_matches_special_scheme(
            pattern.protocol_component);
    if (indexable) {
      if (!ignore_case.has_value()) ignore_case = pattern.ignore_case();
      indexable = pattern.ignore_case() == *ignore_case;
    }
    if (indexable) {
      indexed_patterns_.push_back(i);
      pathnames.push_back(pattern.get_pathname());
    } else {
      unindexed_patterns_.push_back(i);
    }
  }
  if (pathnames.empty()) {
    return;
  }

  ada::url_pattern_options options{.ignore_case = *ignore_case};
  auto index = ada::parse_url_pattern_list<URLPatternRegexProvider>(
      pathnames, nullptr, &options);
  if (!index) {
    // Without an index, FindMatch() tests every pattern in list order.
    indexed_patterns_.clear();
    unindexed_patterns_.clear();
    return;
  }
  pathname_index_.emplace(std::move(*index));
}

bool URLPatternList::Matches(size_t index, const Components& components) const {
  return patterns_[index]->url_pattern_.test_components(components.protocol,
                                                        components.username,
                                                        components.password,
                                                        components.hostname,
                                                        components.port,
                                                        components.pathname,
                                                        components.search,
                                                        components.hash);
}

std::optional<size_t> URLPatternList::FindMatch(const Components& components,
                                                bool any_match) const {
  size_t end = patterns_.size();
  std::optional<size_t> match;
  if (pathname_index_.has_value()) {
    auto result = pathname_index_->match(components.pathname);
    if (!result.has_match()) {
      for (size_t index : unindexed_patterns_) {
        if (Matches(index, components)) return index;
      }
      return std::nullopt;
    }
    // The index reports the most specific matching pathname rather than the
    // first one in list order, and the rest of that pattern may not match.
    // When it does match, only the patterns before it can match first.
    size_t candidate =
        indexed_patterns_[static_cast<size_t>(result.route_index)];
    if (Matches(candidate, components)) {
      if (any_match) return candidate;
      match = candidate;
      end = candidate;
    }
  }
  for (size_t index = 0; index < end; index++) {
    if (Matches(index, components)) return index;
  }
  return match;
}

void URLPatternList::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("patterns", patterns_);
  tracker->TrackField("indexed_patterns", indexed_patterns_);
  tracker->TrackField("unindexed_patterns", unindexed_patterns_);
}

void URLPatternList::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args.IsConstructCall()) {
    THROW_ERR_CONSTRUCT_CALL_REQUIRED(env);
    return;
  }
  if (env->EmitURLPatternListWarning() &&
      ProcessEmitWarningGeneric(env,
                                "URLPatternList is an experimental feature and "
                                "might change at any time",
                                "ExperimentalWarning")
          .IsNothing()) {
    return;
  }
  if (args.Length() < 1) {
    THROW_ERR_MISSING_ARGS(env, "The \"patterns\" argument must be specified");
    return;
  }

  std::vector<BaseObjectPtr<URLPattern>> patterns;
  if (!ToURLPatternSequence(env, args[0], &patterns)) {
    return;
  }

  std::unordered_set<URLPattern*> seen;
  seen.reserve(patterns.size());
  for (const auto& pattern : patterns) {
    if (!seen.insert(pattern.get()).second) {
      THROW_ERR_INVALID_ARG_VALUE(env,
                                  "The \"patterns\" argument must not contain "
                                  "the same URLPattern more than once");
      return;
    }
  }
  if (patterns.empty()) {
    THROW_ERR_INVALID_ARG_VALUE(env,
                                "The \"patterns\" argument must not be empty");
    return;
  }

  new URLPatternList(env, args.This(), std::move(patterns));
}

void URLPatternList::Exec(const FunctionCallbackInfo<Value>& args) {
  URLPatternList* list;
  ASSIGN_OR_RETURN_UNWRAP(&list, args.This());
  Environment* env = Environment::GetCurrent(args);

  URLPatternMatchArgs match_args;
  if (!ParseURLPatternMatchArgs(env, args, &match_args)) return;
  std::optional<std::string_view> base_url = match_args.base_url_view();
  const std::string_view* base_url_ptr = base_url ? &*base_url : nullptr;

  auto components = ProcessMatchInput(match_args.input, base_url_ptr);
  if (!components) {
    THROW_ERR_OPERATION_FAILED(env, "Failed to exec URLPatternList");
    return;
  }
  std::optional<size_t> index;
  if (components->has_value()) {
    index = list->FindMatch(**components, false);
  }
  if (!index.has_value()) {
    args.GetReturnValue().SetNull();
    return;
  }

  URLPattern* pattern = list->patterns_[*index].get();
  auto result = pattern->url_pattern_.exec(match_args.input, base_url_ptr);
  if (!result) {
    THROW_ERR_OPERATION_FAILED(env, "Failed to exec URLPatternList");
    return;
  }
  if (!result->has_value()) {
    args.GetReturnValue().SetNull();
    return;
  }
  Local<Value> js_result;
  if (URLPattern::URLPatternResult::ToJSValue(
          env, result->value(), pattern->object())
          .ToLocal(&js_result)) {
    args.GetReturnValue().Set(js_result);
  }
}

void URLPatternList::Test(const FunctionCallbackInfo<Value>& args) {
  URLPatternList* list;
  ASSIGN_OR_RETURN_UNWRAP(&list, args.This());
  Environment* env = Environment::GetCurrent(args);

  URLPatternMatchArgs match_args;
  if (!ParseURLPatternMatchArgs(env, args, &match_args)) return;
  std::optional<std::string_view> base_url = match_args.base_url_view();

  auto components =
      ProcessMatchInput(match_args.input, base_url ? &*base_url : nullptr);
  if (!components) {
    THROW_ERR_OPERATION_FAILED(env, "Failed to test URLPatternList");
    return;
  }
  args.GetReturnValue().Set(components->has_value() &&
                            list->FindMatch(**components, true).has_value());
}

static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(URLPattern::New);
#define URL_PATTERN_COMPONENT_GETTERS(uppercase_name, _)                       \
  registry->Register(URLPattern::uppercase_name);
  URL_PATTERN_COMPONENTS(URL_PATTERN_COMPONENT_GETTERS)
#undef URL_PATTERN_COMPONENT_GETTERS
  registry->Register(URLPattern::HasRegexpGroups);
  registry->Register(URLPattern::Exec);
  registry->Register(URLPattern::Test);
  registry->Register(URLPatternList::New);
  registry->Register(URLPatternList::Exec);
  registry->Register(URLPatternList::Test);
}

Local<FunctionTemplate> URLPattern::GetConstructorTemplate(Environment* env) {
  Local<FunctionTemplate> ctor_tmpl = env->urlpattern_constructor_template();
  if (!ctor_tmpl.IsEmpty()) {
    return ctor_tmpl;
  }

  Isolate* isolate = env->isolate();
  auto attributes = static_cast<PropertyAttribute>(ReadOnly | DontDelete);
  ctor_tmpl = NewFunctionTemplate(isolate, URLPattern::New);
  auto instance_template = ctor_tmpl->InstanceTemplate();
  auto prototype_template = ctor_tmpl->PrototypeTemplate();
  Local<String> url_pattern_string =
      FIXED_ONE_BYTE_STRING(isolate, "URLPattern");
  ctor_tmpl->SetClassName(url_pattern_string);
  prototype_template->Set(
      v8::Symbol::GetToStringTag(isolate),
      url_pattern_string,
      static_cast<PropertyAttribute>(ReadOnly | v8::DontEnum));

  // The signature is used to prevent the property accessors from being
  // called on the wrong receiver object (`this`)
  auto signature = Signature::New(isolate, ctor_tmpl);

  instance_template->SetInternalFieldCount(URLPattern::kInternalFieldCount);

#define ENV_GETTER(lowercase_name) env->lowercase_name##_string()
#define URL_PATTERN_COMPONENT_GETTERS(uppercase_name, lowercase_name)          \
  prototype_template->SetAccessorProperty(                                     \
      ENV_GETTER(lowercase_name),                                              \
      FunctionTemplate::New(                                                   \
          isolate, URLPattern::uppercase_name, Local<Value>(), signature),     \
      Local<FunctionTemplate>(),                                               \
      attributes);
  URL_PATTERN_COMPONENTS(URL_PATTERN_COMPONENT_GETTERS)
#undef URL_PATTERN_COMPONENT_GETTERS
#undef ENV_GETTER

  prototype_template->SetAccessorProperty(
      env->has_regexp_groups_string(),
      FunctionTemplate::New(
          isolate, URLPattern::HasRegexpGroups, Local<Value>(), signature),
      Local<FunctionTemplate>(),
      attributes);

  SetProtoMethodNoSideEffect(isolate, ctor_tmpl, "exec", URLPattern::Exec);
  SetProtoMethodNoSideEffect(isolate, ctor_tmpl, "test", URLPattern::Test);
  env->set_urlpattern_constructor_template(ctor_tmpl);
  return ctor_tmpl;
}

bool URLPattern::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  // The template is shared by every context of the isolate and may already be
  // instantiated, so its class name is set once in GetConstructorTemplate().
  SetConstructorFunction(context,
                         target,
                         "URLPattern",
                         URLPattern::GetConstructorTemplate(env),
                         SetConstructorFunctionFlag::NONE);

  auto list_tmpl = NewFunctionTemplate(isolate, URLPatternList::New);
  // The `patterns` argument is required.
  list_tmpl->SetLength(1);
  list_tmpl->InstanceTemplate()->SetInternalFieldCount(
      URLPatternList::kInternalFieldCount);
  Local<String> url_pattern_list_string =
      FIXED_ONE_BYTE_STRING(isolate, "URLPatternList");
  list_tmpl->PrototypeTemplate()->Set(
      Symbol::GetToStringTag(isolate),
      url_pattern_list_string,
      static_cast<PropertyAttribute>(ReadOnly | DontEnum));
  SetProtoMethodNoSideEffect(isolate, list_tmpl, "test", URLPatternList::Test);
  SetProtoMethodNoSideEffect(isolate, list_tmpl, "exec", URLPatternList::Exec);
  SetConstructorFunction(context, target, url_pattern_list_string, list_tmpl);
}

}  // namespace node::url_pattern

NODE_BINDING_CONTEXT_AWARE_INTERNAL(url_pattern, node::url_pattern::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(url_pattern,
                                node::url_pattern::RegisterExternalReferences)
