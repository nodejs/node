#include "node_url.h"
#include "ada.h"
#include "base_object-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_i18n.h"
#include "util-inl.h"

#include <cstdio>
#include <numeric>

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Value;

namespace url {
namespace {

enum url_update_action {
  kProtocol = 0,
  kHost = 1,
  kHostname = 2,
  kPort = 3,
  kUsername = 4,
  kPassword = 5,
  kPathname = 6,
  kSearch = 7,
  kHash = 8,
  kHref = 9,
};

auto GetCallbackArgs(Environment* env, const ada::result& url) {
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();

  auto js_string = [&](std::string_view sv) {
    return ToV8Value(context, sv, isolate).ToLocalChecked();
  };
  return std::array{
      js_string(url->get_href()),
      js_string(url->get_origin()),
      js_string(url->get_protocol()),
      js_string(url->get_hostname()),
      js_string(url->get_pathname()),
      js_string(url->get_search()),
      js_string(url->get_username()),
      js_string(url->get_password()),
      js_string(url->get_port()),
      js_string(url->get_hash()),
  };
}

void Parse(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 3);
  CHECK(args[0]->IsString());  // input
  // args[1] // base url
  CHECK(args[2]->IsFunction());  // complete callback

  Local<Function> success_callback_ = args[2].As<Function>();

  Environment* env = Environment::GetCurrent(args);
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Utf8Value input(env->isolate(), args[0]);
  ada::result base;
  ada::url* base_pointer = nullptr;
  if (args[1]->IsString()) {
    base = ada::parse(Utf8Value(env->isolate(), args[1]).ToString());
    if (!base) {
      return args.GetReturnValue().Set(false);
    }
    base_pointer = &base.value();
  }
  ada::result out = ada::parse(input.ToStringView(), base_pointer);

  if (!out) {
    return args.GetReturnValue().Set(false);
  }

  auto argv = GetCallbackArgs(env, out);
  USE(success_callback_->Call(
      env->context(), args.This(), argv.size(), argv.data()));
  args.GetReturnValue().Set(true);
}

void CanParse(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 2);
  CHECK(args[0]->IsString());  // input
  // args[1] // base url

  Environment* env = Environment::GetCurrent(args);
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Utf8Value input(env->isolate(), args[0]);
  ada::result base;
  ada::url* base_pointer = nullptr;
  if (args[1]->IsString()) {
    base = ada::parse(Utf8Value(env->isolate(), args[1]).ToString());
    if (!base) {
      return args.GetReturnValue().Set(false);
    }
    base_pointer = &base.value();
  }
  ada::result out = ada::parse(input.ToStringView(), base_pointer);

  args.GetReturnValue().Set(out.has_value());
}

void DomainToASCII(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  std::string input = Utf8Value(env->isolate(), args[0]).ToString();
  if (input.empty()) {
    return args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(env->isolate(), ""));
  }

#if defined(NODE_HAVE_I18N_SUPPORT)
  // It is important to have an initial value that contains a special scheme.
  // Since it will change the implementation of `set_hostname` according to URL
  // spec.
  ada::result out = ada::parse("ws://x");
  DCHECK(out);
  if (!out->set_hostname(input)) {
    return args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(env->isolate(), ""));
  }
  std::string host = out->get_hostname();
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), host.c_str()).ToLocalChecked());
#else
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), input.c_str()).ToLocalChecked());
#endif
}

void DomainToUnicode(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  std::string input = Utf8Value(env->isolate(), args[0]).ToString();
#if defined(NODE_HAVE_I18N_SUPPORT)
  // It is important to have an initial value that contains a special scheme.
  // Since it will change the implementation of `set_hostname` according to URL
  // spec.
  ada::result out = ada::parse("ws://x");
  DCHECK(out);
  if (!out->set_hostname(input)) {
    return args.GetReturnValue().Set(
        String::NewFromUtf8(env->isolate(), "").ToLocalChecked());
  }
  std::string host = out->get_hostname();

  MaybeStackBuffer<char> buf;
  int32_t len = i18n::ToUnicode(&buf, host.data(), host.length());

  if (len < 0) {
    return args.GetReturnValue().Set(
        String::NewFromUtf8(env->isolate(), "").ToLocalChecked());
  }

  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), *buf, NewStringType::kNormal, len)
          .ToLocalChecked());
#else  // !defined(NODE_HAVE_I18N_SUPPORT)
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), input.c_str()).ToLocalChecked());
#endif
}

void UpdateUrl(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());    // href
  CHECK(args[1]->IsNumber());    // action type
  CHECK(args[2]->IsString());    // new value
  CHECK(args[3]->IsFunction());  // success callback

  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  enum url_update_action action = static_cast<enum url_update_action>(
      args[1]->Uint32Value(env->context()).FromJust());
  Utf8Value input(isolate, args[0].As<String>());
  Utf8Value new_value(isolate, args[2].As<String>());
  Local<Function> success_callback_ = args[3].As<Function>();

  std::string_view new_value_view = new_value.ToStringView();
  ada::result out = ada::parse(input.ToStringView());
  CHECK(out);

  bool result{true};

  switch (action) {
    case kPathname: {
      result = out->set_pathname(new_value_view);
      break;
    }
    case kHash: {
      out->set_hash(new_value_view);
      break;
    }
    case kHost: {
      result = out->set_host(new_value_view);
      break;
    }
    case kHostname: {
      result = out->set_hostname(new_value_view);
      break;
    }
    case kHref: {
      result = out->set_href(new_value_view);
      break;
    }
    case kPassword: {
      result = out->set_password(new_value_view);
      break;
    }
    case kPort: {
      result = out->set_port(new_value_view);
      break;
    }
    case kProtocol: {
      result = out->set_protocol(new_value_view);
      break;
    }
    case kSearch: {
      out->set_search(new_value_view);
      break;
    }
    case kUsername: {
      result = out->set_username(new_value_view);
      break;
    }
  }

  auto argv = GetCallbackArgs(env, out);
  USE(success_callback_->Call(
      env->context(), args.This(), argv.size(), argv.data()));
  args.GetReturnValue().Set(result);
}

void FormatUrl(const FunctionCallbackInfo<Value>& args) {
  CHECK_GT(args.Length(), 4);
  CHECK(args[0]->IsString());  // url href

  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  Utf8Value href(isolate, args[0].As<String>());
  const bool fragment = args[1]->IsTrue();
  const bool unicode = args[2]->IsTrue();
  const bool search = args[3]->IsTrue();
  const bool auth = args[4]->IsTrue();

  ada::result out = ada::parse(href.ToStringView());
  CHECK(out);

  if (!fragment) {
    out->fragment = std::nullopt;
  }

  if (unicode) {
#if defined(NODE_HAVE_I18N_SUPPORT)
    std::string hostname = out->get_hostname();
    MaybeStackBuffer<char> buf;
    int32_t len = i18n::ToUnicode(&buf, hostname.data(), hostname.length());

    if (len < 0) {
      out->host = "";
    } else {
      out->host = buf.ToString();
    }
#else
    out->host = "";
#endif
  }

  if (!search) {
    out->query = std::nullopt;
  }

  if (!auth) {
    out->username = "";
    out->password = "";
  }

  std::string result = out->get_href();
  args.GetReturnValue().Set(String::NewFromUtf8(env->isolate(),
                                                result.data(),
                                                NewStringType::kNormal,
                                                result.length())
                                .ToLocalChecked());
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(context, target, "parse", Parse);
  SetMethod(context, target, "updateUrl", UpdateUrl);
  SetMethodNoSideEffect(context, target, "canParse", CanParse);
  SetMethodNoSideEffect(context, target, "formatUrl", FormatUrl);

  SetMethodNoSideEffect(context, target, "domainToASCII", DomainToASCII);
  SetMethodNoSideEffect(context, target, "domainToUnicode", DomainToUnicode);
}
}  // namespace

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Parse);
  registry->Register(CanParse);
  registry->Register(UpdateUrl);
  registry->Register(FormatUrl);

  registry->Register(DomainToASCII);
  registry->Register(DomainToUnicode);
}

std::string FromFilePath(const std::string_view file_path) {
  std::string escaped_file_path;
  for (size_t i = 0; i < file_path.length(); ++i) {
    escaped_file_path += file_path[i];
    if (file_path[i] == '%') escaped_file_path += "25";
  }
  return ada::href_from_file(escaped_file_path);
}

}  // namespace url
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(url, node::url::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(url, node::url::RegisterExternalReferences)
