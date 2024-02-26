#include "node_code_integrity.h"
#include "v8.h"
#include "node.h"
#include "env-inl.h"
#include "node_external_reference.h"

namespace node {

using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;


namespace codeintegrity {

static void IsCodeIntegrityForcedByOS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  int ret = 0;

#ifdef _WIN32
  HRESULT hr = E_FAIL;

  HMODULE wldp_module = LoadLibraryExA(
    "wldp.dll",
    nullptr,
    LOAD_LIBRARY_SEARCH_SYSTEM32);

  if (wldp_module == nullptr) {
    // this case only happens on Windows versions that don't support
    // code integrity policies.
    args.GetReturnValue().Set(Boolean::New(env->isolate(), false));
    return;
  }

  pfnWldpGetApplicationSettingBoolean WldpGetApplicationSettingBoolean =
  (pfnWldpGetApplicationSettingBoolean)GetProcAddress(
    wldp_module,
    "WldpGetApplicationSettingBoolean");

  if (WldpGetApplicationSettingBoolean != nullptr) {
    HRESULT hr = WldpGetApplicationSettingBoolean(
      L"nodejs",
      L"EnforceCodeIntegrity",
      &ret);

    if (SUCCEEDED(hr)) {
      args.GetReturnValue().Set(
        Boolean::New(env->isolate(), static_cast<bool>(ret)));
      return;
    } else if (hr != 0x80070490) {  // E_NOTFOUND
      args.GetReturnValue().Set(Boolean::New(env->isolate(), false));
      return;
    }
  }

  // WldpGetApplicationSettingBoolean is the preferred way for applications to
  // query security policy values. However, this method only exists on Windows
  // versions going back to circa Win10 2023H2. In order to support systems
  // older than that (down to Win10RS2), we can use the deprecated
  // WldpQuerySecurityPolicy
  pfnWldpQuerySecurityPolicy WldpQuerySecurityPolicy =
    (pfnWldpQuerySecurityPolicy)GetProcAddress(
      wldp_module,
      "WldpQuerySecurityPolicy");

  if (WldpQuerySecurityPolicy != nullptr) {
    DECLARE_CONST_UNICODE_STRING(providerName, L"nodejs");
    DECLARE_CONST_UNICODE_STRING(keyName, L"Settings");
    DECLARE_CONST_UNICODE_STRING(valueName, L"EnforceCodeIntegrity");
    WLDP_SECURE_SETTING_VALUE_TYPE valueType =
      WLDP_SECURE_SETTING_VALUE_TYPE_BOOLEAN;
    ULONG valueSize = sizeof(int);
    hr = WldpQuerySecurityPolicy(
              &providerName,
              &keyName,
              &valueName,
              &valueType,
              &ret,
              &valueSize);
    if (FAILED(hr)) {
      args.GetReturnValue().Set(Boolean::New(env->isolate(), false));
      return;
    }
  }
#endif  // _WIN32
  args.GetReturnValue().Set(Boolean::New(env->isolate(), ret));
}

static void IsFileTrustedBySystemCodeIntegrityPolicy(
  const FunctionCallbackInfo<Value>& args) {
#ifdef _WIN32
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());

  BufferValue manifestPath(env->isolate(), args[0]);
  if (*manifestPath == nullptr) {
    return env->ThrowError("Manifest path cannot be empty");
  }

  BufferValue signaturePath(env->isolate(), args[1]);
  if (*signaturePath == nullptr) {
    return env->ThrowError("Signature path cannot be empty");
  }

  HANDLE hNodePolicyFile = CreateFileA(
    *manifestPath,
    GENERIC_READ,
    FILE_SHARE_READ,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    nullptr);

  if (hNodePolicyFile == INVALID_HANDLE_VALUE || hNodePolicyFile == nullptr) {
    return env->ThrowError("invalid manifest path");
  }

  HANDLE hSignatureFile = CreateFileA(
    *signaturePath,
    GENERIC_READ,
    FILE_SHARE_READ,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    nullptr);

  if (hSignatureFile == INVALID_HANDLE_VALUE || hSignatureFile == nullptr) {
    return env->ThrowError("invalid signature path");
  }

  HMODULE wldp_module = LoadLibraryExA(
    "wldp.dll",
    nullptr,
    LOAD_LIBRARY_SEARCH_SYSTEM32);

  if (wldp_module == nullptr) {
    return env->ThrowError("Unable to load wldp.dll");
  }

  pfnWldpCanExecuteFileFromDetachedSignature
    WldpCanExecuteFileFromDetachedSignature =
    (pfnWldpCanExecuteFileFromDetachedSignature)GetProcAddress(
      wldp_module,
      "WldpCanExecuteFileFromDetachedSignature");

  if (WldpCanExecuteFileFromDetachedSignature == nullptr) {
    return env->ThrowError(
      "Cannot find proc WldpCanExecuteFileFromDetachedSignature");
  }

  const GUID wldp_host_other = WLDP_HOST_OTHER;
  WLDP_EXECUTION_POLICY result;
  HRESULT hr = WldpCanExecuteFileFromDetachedSignature(
    wldp_host_other,
    WLDP_EXECUTION_EVALUATION_OPTION_NONE,
    hNodePolicyFile,
    hSignatureFile,
    nullptr,
    &result);

  if (FAILED(hr)) {
    return env->ThrowError("WldpCanExecuteFileFromDetachedSignature failed");
  }

  bool isPolicyTrusted = (result == WLDP_EXECUTION_POLICY_ALLOWED);
  args.GetReturnValue().Set(isPolicyTrusted);
#endif  // _WIN32
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(
    context,
    target,
    "isCodeIntegrityForcedByOS",
    IsCodeIntegrityForcedByOS);

  SetMethod(
    context,
    target,
    "isFileTrustedBySystemCodeIntegrityPolicy",
    IsFileTrustedBySystemCodeIntegrityPolicy);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
//   BindingData::RegisterExternalReferences(registry);

  registry->Register(IsCodeIntegrityForcedByOS);
  registry->Register(IsFileTrustedBySystemCodeIntegrityPolicy);
}

}  // namespace codeintegrity
}  // namespace node
NODE_BINDING_CONTEXT_AWARE_INTERNAL(code_integrity,
                                    node::codeintegrity::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(code_integrity,
                            node::codeintegrity::RegisterExternalReferences)
