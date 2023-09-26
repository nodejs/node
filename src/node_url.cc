#include "node_url.h"
#include "ada.h"
#include "base_object-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_i18n.h"
#include "util-inl.h"
#include "v8-fast-api-calls.h"
#include "v8.h"

#include <cstdint>
#include <cstdio>
#include <numeric>

namespace node {
namespace url {

using v8::CFunction;
using v8::Context;
using v8::FastOneByteString;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Value;

void BindingData::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("url_components_buffer", url_components_buffer_);
}

BindingData::BindingData(Realm* realm, v8::Local<v8::Object> object)
    : SnapshotableObject(realm, object, type_int),
      url_components_buffer_(realm->isolate(), kURLComponentsLength) {
  object
      ->Set(realm->context(),
            FIXED_ONE_BYTE_STRING(realm->isolate(), "urlComponents"),
            url_components_buffer_.GetJSArray())
      .Check();
  url_components_buffer_.MakeWeak();
}

bool BindingData::PrepareForSerialization(v8::Local<v8::Context> context,
                                          v8::SnapshotCreator* creator) {
  // We'll just re-initialize the buffers in the constructor since their
  // contents can be thrown away once consumed in the previous call.
  url_components_buffer_.Release();
  // Return true because we need to maintain the reference to the binding from
  // JS land.
  return true;
}

InternalFieldInfoBase* BindingData::Serialize(int index) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  InternalFieldInfo* info =
      InternalFieldInfoBase::New<InternalFieldInfo>(type());
  return info;
}

void BindingData::Deserialize(v8::Local<v8::Context> context,
                              v8::Local<v8::Object> holder,
                              int index,
                              InternalFieldInfoBase* info) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  v8::HandleScope scope(context->GetIsolate());
  Realm* realm = Realm::GetCurrent(context);
  BindingData* binding = realm->AddBindingData<BindingData>(holder);
  CHECK_NOT_NULL(binding);
}

void BindingData::DomainToASCII(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  std::string input = Utf8Value(env->isolate(), args[0]).ToString();
  if (input.empty()) {
    return args.GetReturnValue().Set(String::Empty(env->isolate()));
  }

  // It is important to have an initial value that contains a special scheme.
  // Since it will change the implementation of `set_hostname` according to URL
  // spec.
  auto out = ada::parse<ada::url>("ws://x");
  DCHECK(out);
  if (!out->set_hostname(input)) {
    return args.GetReturnValue().Set(String::Empty(env->isolate()));
  }
  std::string host = out->get_hostname();
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), host.c_str()).ToLocalChecked());
}

void BindingData::DomainToUnicode(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  std::string input = Utf8Value(env->isolate(), args[0]).ToString();
  if (input.empty()) {
    return args.GetReturnValue().Set(String::Empty(env->isolate()));
  }

  // It is important to have an initial value that contains a special scheme.
  // Since it will change the implementation of `set_hostname` according to URL
  // spec.
  auto out = ada::parse<ada::url>("ws://x");
  DCHECK(out);
  if (!out->set_hostname(input)) {
    return args.GetReturnValue().Set(String::Empty(env->isolate()));
  }
  std::string result = ada::unicode::to_unicode(out->get_hostname());

  args.GetReturnValue().Set(String::NewFromUtf8(env->isolate(),
                                                result.c_str(),
                                                NewStringType::kNormal,
                                                result.length())
                                .ToLocalChecked());
}

void BindingData::GetOrigin(const v8::FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());  // input

  Environment* env = Environment::GetCurrent(args);
  HandleScope handle_scope(env->isolate());

  Utf8Value input(env->isolate(), args[0]);
  std::string_view input_view = input.ToStringView();
  auto out = ada::parse<ada::url_aggregator>(input_view);

  if (!out) {
    THROW_ERR_INVALID_URL(env, "Invalid URL");
    return;
  }

  std::string origin = out->get_origin();
  args.GetReturnValue().Set(String::NewFromUtf8(env->isolate(),
                                                origin.data(),
                                                NewStringType::kNormal,
                                                origin.length())
                                .ToLocalChecked());
}

void BindingData::CanParse(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());  // input
  // args[1] // base url

  Environment* env = Environment::GetCurrent(args);
  HandleScope handle_scope(env->isolate());

  Utf8Value input(env->isolate(), args[0]);
  std::string_view input_view = input.ToStringView();

  bool can_parse{};
  if (args[1]->IsString()) {
    Utf8Value base(env->isolate(), args[1]);
    std::string_view base_view = base.ToStringView();
    can_parse = ada::can_parse(input_view, &base_view);
  } else {
    can_parse = ada::can_parse(input_view);
  }

  args.GetReturnValue().Set(can_parse);
}

bool BindingData::FastCanParse(Local<Value> receiver,
                               const FastOneByteString& input) {
  return ada::can_parse(std::string_view(input.data, input.length));
}

bool BindingData::FastCanParseWithBase(Local<Value> receiver,
                                       const FastOneByteString& input,
                                       const FastOneByteString& base) {
  auto base_view = std::string_view(base.data, base.length);
  return ada::can_parse(std::string_view(input.data, input.length), &base_view);
}

CFunction BindingData::fast_can_parse_methods_[] = {
    CFunction::Make(FastCanParse), CFunction::Make(FastCanParseWithBase)};

void BindingData::Format(const FunctionCallbackInfo<Value>& args) {
  CHECK_GT(args.Length(), 4);
  CHECK(args[0]->IsString());  // url href

  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  Utf8Value href(isolate, args[0].As<String>());
  const bool hash = args[1]->IsTrue();
  const bool unicode = args[2]->IsTrue();
  const bool search = args[3]->IsTrue();
  const bool auth = args[4]->IsTrue();

  // ada::url provides a faster alternative to ada::url_aggregator if we
  // directly want to manipulate the url components without using the respective
  // setters. therefore we are using ada::url here.
  auto out = ada::parse<ada::url>(href.ToStringView());
  CHECK(out);

  if (!hash) {
    out->hash = std::nullopt;
  }

  if (unicode && out->has_hostname()) {
    out->host = ada::idna::to_unicode(out->get_hostname());
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

void BindingData::ThrowInvalidURL(node::Environment* env,
                                  std::string_view input,
                                  std::optional<std::string> base) {
  Local<Value> err = ERR_INVALID_URL(env->isolate(), "Invalid URL");
  DCHECK(err->IsObject());

  auto err_object = err.As<Object>();

  USE(err_object->Set(env->context(),
                      env->input_string(),
                      v8::String::NewFromUtf8(env->isolate(),
                                              input.data(),
                                              v8::NewStringType::kNormal,
                                              input.size())
                          .ToLocalChecked()));

  if (base.has_value()) {
    USE(err_object->Set(env->context(),
                        env->base_string(),
                        v8::String::NewFromUtf8(env->isolate(),
                                                base.value().c_str(),
                                                v8::NewStringType::kNormal,
                                                base.value().size())
                            .ToLocalChecked()));
  }

  env->isolate()->ThrowException(err);
}

void BindingData::Parse(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());  // input
  // args[1] // base url

  Realm* realm = Realm::GetCurrent(args);
  BindingData* binding_data = realm->GetBindingData<BindingData>();
  Isolate* isolate = realm->isolate();
  std::optional<std::string> base_{};

  Utf8Value input(isolate, args[0]);
  ada::result<ada::url_aggregator> base;
  ada::url_aggregator* base_pointer = nullptr;
  if (args[1]->IsString()) {
    base_ = Utf8Value(isolate, args[1]).ToString();
    base = ada::parse<ada::url_aggregator>(*base_);
    if (!base) {
      return ThrowInvalidURL(realm->env(), input.ToStringView(), base_);
    }
    base_pointer = &base.value();
  }
  auto out =
      ada::parse<ada::url_aggregator>(input.ToStringView(), base_pointer);

  if (!out) {
    return ThrowInvalidURL(realm->env(), input.ToStringView(), base_);
  }

  binding_data->UpdateComponents(out->get_components(), out->type);

  args.GetReturnValue().Set(
      ToV8Value(realm->context(), out->get_href(), isolate).ToLocalChecked());
}

void BindingData::Update(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());    // href
  CHECK(args[1]->IsNumber());    // action type
  CHECK(args[2]->IsString());    // new value

  Realm* realm = Realm::GetCurrent(args);
  BindingData* binding_data = realm->GetBindingData<BindingData>();
  Isolate* isolate = realm->isolate();

  enum url_update_action action = static_cast<enum url_update_action>(
      args[1]->Uint32Value(realm->context()).FromJust());
  Utf8Value input(isolate, args[0].As<String>());
  Utf8Value new_value(isolate, args[2].As<String>());

  std::string_view new_value_view = new_value.ToStringView();
  auto out = ada::parse<ada::url_aggregator>(input.ToStringView());
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
    default:
      UNREACHABLE("Unsupported URL update action");
  }

  if (!result) {
    return args.GetReturnValue().Set(false);
  }

  binding_data->UpdateComponents(out->get_components(), out->type);
  args.GetReturnValue().Set(
      ToV8Value(realm->context(), out->get_href(), isolate).ToLocalChecked());
}

void BindingData::UpdateComponents(const ada::url_components& components,
                                   const ada::scheme::type type) {
  url_components_buffer_[0] = components.protocol_end;
  url_components_buffer_[1] = components.username_end;
  url_components_buffer_[2] = components.host_start;
  url_components_buffer_[3] = components.host_end;
  url_components_buffer_[4] = components.port;
  url_components_buffer_[5] = components.pathname_start;
  url_components_buffer_[6] = components.search_start;
  url_components_buffer_[7] = components.hash_start;
  url_components_buffer_[8] = type;
  static_assert(kURLComponentsLength == 9,
                "kURLComponentsLength should be up-to-date");
}

void BindingData::CreatePerIsolateProperties(IsolateData* isolate_data,
                                             Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  SetMethodNoSideEffect(isolate, target, "domainToASCII", DomainToASCII);
  SetMethodNoSideEffect(isolate, target, "domainToUnicode", DomainToUnicode);
  SetMethodNoSideEffect(isolate, target, "format", Format);
  SetMethodNoSideEffect(isolate, target, "getOrigin", GetOrigin);
  SetMethod(isolate, target, "parse", Parse);
  SetMethod(isolate, target, "update", Update);
  SetFastMethodNoSideEffect(
      isolate, target, "canParse", CanParse, {fast_can_parse_methods_, 2});
}

void BindingData::CreatePerContextProperties(Local<Object> target,
                                             Local<Value> unused,
                                             Local<Context> context,
                                             void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  realm->AddBindingData<BindingData>(target);
}

void BindingData::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(DomainToASCII);
  registry->Register(DomainToUnicode);
  registry->Register(Format);
  registry->Register(GetOrigin);
  registry->Register(Parse);
  registry->Register(Update);
  registry->Register(CanParse);
  registry->Register(FastCanParse);
  registry->Register(FastCanParseWithBase);

  for (const CFunction& method : fast_can_parse_methods_) {
    registry->Register(method.GetTypeInfo());
  }
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

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    url, node::url::BindingData::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(
    url, node::url::BindingData::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    url, node::url::BindingData::RegisterExternalReferences)
