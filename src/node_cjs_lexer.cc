#include "env-inl.h"
#include "node_external_reference.h"
#include "simdutf.h"
#include "util-inl.h"
#include "v8.h"

#include "merve.h"

namespace node {
namespace cjs_lexer {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::NewStringType;
using v8::Object;
using v8::Set;
using v8::String;
using v8::Value;

// Create a V8 string from an export_string variant, using fast path for ASCII
inline Local<String> CreateString(Isolate* isolate,
                                  const lexer::export_string& str) {
  std::string_view sv = lexer::get_string_view(str);

  if (simdutf::validate_ascii(sv.data(), sv.size())) {
    return String::NewFromOneByte(isolate,
                                  reinterpret_cast<const uint8_t*>(sv.data()),
                                  NewStringType::kInternalized,
                                  static_cast<int>(sv.size()))
        .ToLocalChecked();
  }

  return String::NewFromUtf8(isolate,
                             sv.data(),
                             NewStringType::kInternalized,
                             static_cast<int>(sv.size()))
      .ToLocalChecked();
}

void Parse(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  // Handle null, undefined, or missing argument by defaulting to empty string
  if (args.Length() < 1 || args[0]->IsNullOrUndefined()) {
    Local<Value> result_elements[] = {Set::New(isolate),
                                      Array::New(isolate, 0)};
    args.GetReturnValue().Set(Array::New(isolate, result_elements, 2));
    return;
  }

  CHECK(args[0]->IsString());

  Utf8Value source(isolate, args[0]);

  auto result = lexer::parse_commonjs(source.ToStringView());

  if (!result.has_value()) {
    Local<Value> result_elements[] = {Set::New(isolate),
                                      Array::New(isolate, 0)};
    args.GetReturnValue().Set(Array::New(isolate, result_elements, 2));
    return;
  }

  const auto& analysis = result.value();

  // Convert exports to JS Set
  Local<Set> exports_set = Set::New(isolate);
  for (const auto& exp : analysis.exports) {
    exports_set->Add(context, CreateString(isolate, exp)).ToLocalChecked();
  }

  // Convert reexports to JS array using batch creation
  LocalVector<Value> reexports_vec(isolate);
  reexports_vec.reserve(analysis.re_exports.size());
  for (const auto& reexp : analysis.re_exports) {
    reexports_vec.push_back(CreateString(isolate, reexp));
  }

  // Create result array [exports (Set), reexports (Array)]
  Local<Value> result_elements[] = {
      exports_set,
      Array::New(isolate, reexports_vec.data(), reexports_vec.size())};
  args.GetReturnValue().Set(Array::New(isolate, result_elements, 2));
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethodNoSideEffect(context, target, "parse", Parse);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Parse);
}

}  // namespace cjs_lexer
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(cjs_lexer, node::cjs_lexer::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(cjs_lexer,
                                node::cjs_lexer::RegisterExternalReferences)
