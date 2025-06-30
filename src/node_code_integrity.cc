#ifdef _WIN32

#include "node_code_integrity.h"
#include "env-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util.h"
#include "v8.h"

namespace node {

using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

namespace per_process {
bool isWldpInitialized = false;

// WldpCanExecuteFile queries system code integrity policy
// to determine if the contents of a file are allowed to be executed.
pfnWldpCanExecuteFile WldpCanExecuteFile;

// WldpGetApplicationSettingBoolean queries system code integrity policy
// for an arbitrary flag. NodeJS uses the "Node.js EnforceCodeIntegrity"
// flag to determine if NodeJS should be calling WldpCanExecuteFile
// on files intended for execution
// NodeJS also uses the "Node.js DisableInteractiveMode" flag to determine
// if it should restrict interactive code execution. More details
// on how to configure these flags can be found in doc/api/code_integrity.md
pfnWldpGetApplicationSettingBoolean WldpGetApplicationSettingBoolean;

// WldpQuerySecurityPolicy performs similar functionality to
// WldpGetApplicationSettingBoolean, except for legacy Windows systems.
//  WldpGetApplicationSettingBoolean was introduced  Win10 2023H2,
// and is the modern API. However, to support more Node users,
// we also fall back to WldpQuerySecurityPolicy,
// which is available on Windows systems back to Win10 RS2
pfnWldpQuerySecurityPolicy WldpQuerySecurityPolicy;
}  // namespace per_process

namespace code_integrity {

static PCWSTR NODEJS = L"Node.js";
static PCWSTR ENFORCE_CODE_INTEGRITY_SETTING_NAME = L"EnforceCodeIntegrity";
static PCWSTR DISABLE_INTERPRETIVE_MODE_SETTING_NAME =
    L"DisableInteractiveMode";

// InitWldp loads WLDP.dll (the Windows code integrity for interpreters DLL)
// and the relevant function pointers
void InitWldp(Environment* env) {
  if (per_process::isWldpInitialized) {
    return;
  }

  HMODULE wldp_module =
      LoadLibraryExA("wldp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

  if (wldp_module == nullptr) {
    // Wldp is included on all Windows systems that are supported by Node.js
    // If Wldp is unable to be loaded, something is very wrong with
    // the system state
    THROW_ERR_INVALID_STATE(env, "WLDP.DLL does not exist");
    return;
  }

  per_process::WldpCanExecuteFile =
      (pfnWldpCanExecuteFile)GetProcAddress(wldp_module, "WldpCanExecuteFile");

  per_process::WldpGetApplicationSettingBoolean =
      (pfnWldpGetApplicationSettingBoolean)GetProcAddress(
          wldp_module, "WldpGetApplicationSettingBoolean");

  per_process::WldpQuerySecurityPolicy =
      (pfnWldpQuerySecurityPolicy)GetProcAddress(wldp_module,
                                                 "WldpQuerySecurityPolicy");

  per_process::isWldpInitialized = true;
}

// IsFileTrustedBySystemCodeIntegrityPolicy
// Queries operating system to determine if the contents of a file are
// allowed to be executed according to system code integrity policy.
static void IsFileTrustedBySystemCodeIntegrityPolicy(
    const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  Environment* env = Environment::GetCurrent(args);
  if (!per_process::isWldpInitialized) {
    InitWldp(env);
  }

  BufferValue path(env->isolate(), args[0]);
  CHECK_NOT_NULL(*path);

  HANDLE hFile = CreateFileA(*path,
                             GENERIC_READ,
                             FILE_SHARE_READ,
                             nullptr,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             nullptr);

  if (hFile == INVALID_HANDLE_VALUE || hFile == nullptr) {
    return args.GetReturnValue().SetFalse();
  }

  const GUID wldp_host_other = WLDP_HOST_OTHER;
  WLDP_EXECUTION_POLICY result;
  HRESULT hr =
      per_process::WldpCanExecuteFile(wldp_host_other,
                                      WLDP_EXECUTION_EVALUATION_OPTION_NONE,
                                      hFile,
                                      NODEJS,
                                      &result);
  CloseHandle(hFile);

  if (FAILED(hr)) {
    // The failure cases from WldpCanExecuteFile are generally
    // not recoverable. Inspection of the Windows event logs is necessary.
    // The secure failure mode is not executing the file
    args.GetReturnValue().SetFalse();
    return;
  }

  bool isFileTrusted = (result == WLDP_EXECUTION_POLICY_ALLOWED);
  args.GetReturnValue().Set(isFileTrusted);
}

// IsInteractiveModeDisabled
// Queries operating system code integrity policy to determine if
// the policy is requesting NodeJS to disable interactive mode.
static void IsInteractiveModeDisabled(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(args.Length(), 0);

  Environment* env = Environment::GetCurrent(args);

  if (!per_process::isWldpInitialized) {
    InitWldp(env);
  }

  if (per_process::WldpGetApplicationSettingBoolean != nullptr) {
    bool isInteractiveModeDisabled;
    HRESULT hr = per_process::WldpGetApplicationSettingBoolean(
        NODEJS,
        DISABLE_INTERPRETIVE_MODE_SETTING_NAME,
        &isInteractiveModeDisabled);

    if (SUCCEEDED(hr)) {
      args.GetReturnValue().Set(isInteractiveModeDisabled);
      return;
    } else if (hr != E_NOTFOUND) {
      // If the setting is not found, continue through to attempt
      // WldpQuerySecurityPolicy, as the setting may be defined
      // in the old settings format
      args.GetReturnValue().SetFalse();
      return;
    }
  }

  // WldpGetApplicationSettingBoolean is the preferred way for applications to
  // query security policy values. However, this method only exists on Windows
  // versions going back to circa Win10 2023H2. In order to support systems
  // older than that (down to Win10RS2), we can use the deprecated
  // WldpQuerySecurityPolicy
  if (per_process::WldpQuerySecurityPolicy != nullptr) {
    DECLARE_CONST_UNICODE_STRING(providerName, L"Node.js");
    DECLARE_CONST_UNICODE_STRING(keyName, L"Settings");
    DECLARE_CONST_UNICODE_STRING(valueName, L"DisableInteractiveMode");
    WLDP_SECURE_SETTING_VALUE_TYPE valueType =
        WLDP_SECURE_SETTING_VALUE_TYPE_BOOLEAN;
    ULONG valueSize = sizeof(int);
    int isInteractiveModeDisabled = 0;
    HRESULT hr =
        per_process::WldpQuerySecurityPolicy(&providerName,
                                             &keyName,
                                             &valueName,
                                             &valueType,
                                             &isInteractiveModeDisabled,
                                             &valueSize);

    if (FAILED(hr)) {
      args.GetReturnValue().SetFalse();
      return;
    }

    args.GetReturnValue().Set(Boolean::New(
        env->isolate(), static_cast<bool>(isInteractiveModeDisabled)));
  }
}

// IsSystemEnforcingCodeIntegrity
// Queries the operating system to determine if NodeJS should be enforcing
// integrity checks by calling WldpCanExecuteFile
static void IsSystemEnforcingCodeIntegrity(
    const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(args.Length(), 0);

  Environment* env = Environment::GetCurrent(args);

  if (!per_process::isWldpInitialized) {
    InitWldp(env);
  }

  if (per_process::WldpGetApplicationSettingBoolean != nullptr) {
    bool isCodeIntegrityEnforced;
    HRESULT hr = per_process::WldpGetApplicationSettingBoolean(
        NODEJS, ENFORCE_CODE_INTEGRITY_SETTING_NAME, &isCodeIntegrityEnforced);

    if (SUCCEEDED(hr)) {
      args.GetReturnValue().Set(isCodeIntegrityEnforced);
      return;
    } else if (hr != E_NOTFOUND) {
      // If the setting is not found, continue through to attempt
      // WldpQuerySecurityPolicy, as the setting may be defined
      // in the old settings format
      args.GetReturnValue().SetFalse();
      return;
    }
  }

  // WldpGetApplicationSettingBoolean is the preferred way for applications to
  // query security policy values. However, this method only exists on Windows
  // versions going back to circa Win10 2023H2. In order to support systems
  // older than that (down to Win10RS2), we can use the deprecated
  // WldpQuerySecurityPolicy
  if (per_process::WldpQuerySecurityPolicy != nullptr) {
    DECLARE_CONST_UNICODE_STRING(providerName, L"Node.js");
    DECLARE_CONST_UNICODE_STRING(keyName, L"Settings");
    DECLARE_CONST_UNICODE_STRING(valueName, L"EnforceCodeIntegrity");
    WLDP_SECURE_SETTING_VALUE_TYPE valueType =
        WLDP_SECURE_SETTING_VALUE_TYPE_BOOLEAN;
    ULONG valueSize = sizeof(int);
    int isCodeIntegrityEnforced = 0;
    HRESULT hr = per_process::WldpQuerySecurityPolicy(&providerName,
                                                      &keyName,
                                                      &valueName,
                                                      &valueType,
                                                      &isCodeIntegrityEnforced,
                                                      &valueSize);

    if (FAILED(hr)) {
      args.GetReturnValue().SetFalse();
      return;
    }

    args.GetReturnValue().Set(Boolean::New(
        env->isolate(), static_cast<bool>(isCodeIntegrityEnforced)));
  }
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(context,
            target,
            "isFileTrustedBySystemCodeIntegrityPolicy",
            IsFileTrustedBySystemCodeIntegrityPolicy);

  SetMethod(
      context, target, "isInteractiveModeDisabled", IsInteractiveModeDisabled);

  SetMethod(context,
            target,
            "isSystemEnforcingCodeIntegrity",
            IsSystemEnforcingCodeIntegrity);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(IsFileTrustedBySystemCodeIntegrityPolicy);
  registry->Register(IsInteractiveModeDisabled);
  registry->Register(IsSystemEnforcingCodeIntegrity);
}

}  // namespace code_integrity
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(code_integrity,
                                    node::code_integrity::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    code_integrity, node::code_integrity::RegisterExternalReferences)
#endif  // _WIN32
