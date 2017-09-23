#include <node.h>
#include <unicode/timezone.h>
#include <unicode/utypes.h>
#include <unicode/uversion.h>

namespace {

inline void Initialize(v8::Local<v8::Object> exports,
                       v8::Local<v8::Value> module,
                       v8::Local<v8::Context> context) {
  auto isolate = context->GetIsolate();
  {
    auto key = v8::String::NewFromUtf8(isolate, "icuVersion");
    auto value = v8::String::NewFromUtf8(isolate, U_ICU_VERSION);
    exports->Set(context, key, value).FromJust();
  }
  {
    UErrorCode tzdata_status = U_ZERO_ERROR;
    const char* tzdata_version = TimeZone::getTZDataVersion(tzdata_status);
    if (U_SUCCESS(tzdata_status) && tzdata_version != nullptr) {
      auto key = v8::String::NewFromUtf8(isolate, "tzdataVersion");
      auto value = v8::String::NewFromUtf8(isolate, tzdata_version);
      exports->Set(context, key, value).FromJust();
    }
  }
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(binding, Initialize)
