#include <node.h>
#include <unicode/ulocdata.h>
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
    UVersionInfo cldr_versions;
    UErrorCode cldr_status = U_ZERO_ERROR;
    ulocdata_getCLDRVersion(cldr_versions, &cldr_status);
    assert(U_SUCCESS(cldr_status));
    char cldr_version[U_MAX_VERSION_STRING_LENGTH];
    u_versionToString(cldr_versions, cldr_version);
    auto key = v8::String::NewFromUtf8(isolate, "cldrVersion");
    auto value = v8::String::NewFromUtf8(isolate, cldr_version);
    exports->Set(context, key, value).FromJust();
  }
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(binding, Initialize)
