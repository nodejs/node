#include "node_internals.h"

#ifdef NODE_IMPLEMENTS_POSIX_CREDENTIALS
#include <grp.h>  // getgrnam()
#include <pwd.h>  // getpwnam()
#endif            // NODE_IMPLEMENTS_POSIX_CREDENTIALS

#if !defined(_MSC_VER)
#include <unistd.h>  // setuid, getuid
#endif

namespace node {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

namespace per_process {
bool linux_at_secure = false;
}  // namespace per_process

namespace credentials {

// Look up environment variable unless running as setuid root.
bool SafeGetenv(const char* key, std::string* text) {
#if !defined(__CloudABI__) && !defined(_WIN32)
  if (per_process::linux_at_secure || getuid() != geteuid() ||
      getgid() != getegid())
    goto fail;
#endif

  {
    Mutex::ScopedLock lock(per_process::env_var_mutex);
    if (const char* value = getenv(key)) {
      *text = value;
      return true;
    }
  }

fail:
  text->clear();
  return false;
}

static void SafeGetenv(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  Isolate* isolate = args.GetIsolate();
  Utf8Value strenvtag(isolate, args[0]);
  std::string text;
  if (!SafeGetenv(*strenvtag, &text)) return;
  Local<Value> result =
      ToV8Value(isolate->GetCurrentContext(), text).ToLocalChecked();
  args.GetReturnValue().Set(result);
}

#ifdef NODE_IMPLEMENTS_POSIX_CREDENTIALS

static const uid_t uid_not_found = static_cast<uid_t>(-1);
static const gid_t gid_not_found = static_cast<gid_t>(-1);

static uid_t uid_by_name(const char* name) {
  struct passwd pwd;
  struct passwd* pp;
  char buf[8192];

  errno = 0;
  pp = nullptr;

  if (getpwnam_r(name, &pwd, buf, sizeof(buf), &pp) == 0 && pp != nullptr)
    return pp->pw_uid;

  return uid_not_found;
}

static char* name_by_uid(uid_t uid) {
  struct passwd pwd;
  struct passwd* pp;
  char buf[8192];
  int rc;

  errno = 0;
  pp = nullptr;

  if ((rc = getpwuid_r(uid, &pwd, buf, sizeof(buf), &pp)) == 0 &&
      pp != nullptr) {
    return strdup(pp->pw_name);
  }

  if (rc == 0) errno = ENOENT;

  return nullptr;
}

static gid_t gid_by_name(const char* name) {
  struct group pwd;
  struct group* pp;
  char buf[8192];

  errno = 0;
  pp = nullptr;

  if (getgrnam_r(name, &pwd, buf, sizeof(buf), &pp) == 0 && pp != nullptr)
    return pp->gr_gid;

  return gid_not_found;
}

#if 0  // For future use.
static const char* name_by_gid(gid_t gid) {
  struct group pwd;
  struct group* pp;
  char buf[8192];
  int rc;

  errno = 0;
  pp = nullptr;

  if ((rc = getgrgid_r(gid, &pwd, buf, sizeof(buf), &pp)) == 0 &&
      pp != nullptr) {
    return strdup(pp->gr_name);
  }

  if (rc == 0)
    errno = ENOENT;

  return nullptr;
}
#endif

static uid_t uid_by_name(Isolate* isolate, Local<Value> value) {
  if (value->IsUint32()) {
    return static_cast<uid_t>(value.As<Uint32>()->Value());
  } else {
    Utf8Value name(isolate, value);
    return uid_by_name(*name);
  }
}

static gid_t gid_by_name(Isolate* isolate, Local<Value> value) {
  if (value->IsUint32()) {
    return static_cast<gid_t>(value.As<Uint32>()->Value());
  } else {
    Utf8Value name(isolate, value);
    return gid_by_name(*name);
  }
}

static void GetUid(const FunctionCallbackInfo<Value>& args) {
  // uid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(getuid()));
}

static void GetGid(const FunctionCallbackInfo<Value>& args) {
  // gid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(getgid()));
}

static void GetEUid(const FunctionCallbackInfo<Value>& args) {
  // uid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(geteuid()));
}

static void GetEGid(const FunctionCallbackInfo<Value>& args) {
  // gid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(getegid()));
}

static void SetGid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(env->owns_process_state());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsUint32() || args[0]->IsString());

  gid_t gid = gid_by_name(env->isolate(), args[0]);

  if (gid == gid_not_found) {
    // Tells JS to throw ERR_INVALID_CREDENTIAL
    args.GetReturnValue().Set(1);
  } else if (setgid(gid)) {
    env->ThrowErrnoException(errno, "setgid");
  } else {
    args.GetReturnValue().Set(0);
  }
}

static void SetEGid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(env->owns_process_state());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsUint32() || args[0]->IsString());

  gid_t gid = gid_by_name(env->isolate(), args[0]);

  if (gid == gid_not_found) {
    // Tells JS to throw ERR_INVALID_CREDENTIAL
    args.GetReturnValue().Set(1);
  } else if (setegid(gid)) {
    env->ThrowErrnoException(errno, "setegid");
  } else {
    args.GetReturnValue().Set(0);
  }
}

static void SetUid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(env->owns_process_state());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsUint32() || args[0]->IsString());

  uid_t uid = uid_by_name(env->isolate(), args[0]);

  if (uid == uid_not_found) {
    // Tells JS to throw ERR_INVALID_CREDENTIAL
    args.GetReturnValue().Set(1);
  } else if (setuid(uid)) {
    env->ThrowErrnoException(errno, "setuid");
  } else {
    args.GetReturnValue().Set(0);
  }
}

static void SetEUid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(env->owns_process_state());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsUint32() || args[0]->IsString());

  uid_t uid = uid_by_name(env->isolate(), args[0]);

  if (uid == uid_not_found) {
    // Tells JS to throw ERR_INVALID_CREDENTIAL
    args.GetReturnValue().Set(1);
  } else if (seteuid(uid)) {
    env->ThrowErrnoException(errno, "seteuid");
  } else {
    args.GetReturnValue().Set(0);
  }
}

static void GetGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  int ngroups = getgroups(0, nullptr);
  if (ngroups == -1) return env->ThrowErrnoException(errno, "getgroups");

  std::vector<gid_t> groups(ngroups);

  ngroups = getgroups(ngroups, groups.data());
  if (ngroups == -1)
    return env->ThrowErrnoException(errno, "getgroups");

  groups.resize(ngroups);
  gid_t egid = getegid();
  if (std::find(groups.begin(), groups.end(), egid) == groups.end())
    groups.push_back(egid);
  MaybeLocal<Value> array = ToV8Value(env->context(), groups);
  if (!array.IsEmpty())
    args.GetReturnValue().Set(array.ToLocalChecked());
}

static void SetGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsArray());

  Local<Array> groups_list = args[0].As<Array>();
  size_t size = groups_list->Length();
  gid_t* groups = new gid_t[size];

  for (size_t i = 0; i < size; i++) {
    gid_t gid = gid_by_name(
        env->isolate(), groups_list->Get(env->context(), i).ToLocalChecked());

    if (gid == gid_not_found) {
      delete[] groups;
      // Tells JS to throw ERR_INVALID_CREDENTIAL
      args.GetReturnValue().Set(static_cast<uint32_t>(i + 1));
      return;
    }

    groups[i] = gid;
  }

  int rc = setgroups(size, groups);
  delete[] groups;

  if (rc == -1) return env->ThrowErrnoException(errno, "setgroups");

  args.GetReturnValue().Set(0);
}

static void InitGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 2);
  CHECK(args[0]->IsUint32() || args[0]->IsString());
  CHECK(args[1]->IsUint32() || args[1]->IsString());

  Utf8Value arg0(env->isolate(), args[0]);
  gid_t extra_group;
  bool must_free;
  char* user;

  if (args[0]->IsUint32()) {
    user = name_by_uid(args[0].As<Uint32>()->Value());
    must_free = true;
  } else {
    user = *arg0;
    must_free = false;
  }

  if (user == nullptr) {
    // Tells JS to throw ERR_INVALID_CREDENTIAL
    return args.GetReturnValue().Set(1);
  }

  extra_group = gid_by_name(env->isolate(), args[1]);

  if (extra_group == gid_not_found) {
    if (must_free) free(user);
    // Tells JS to throw ERR_INVALID_CREDENTIAL
    return args.GetReturnValue().Set(2);
  }

  int rc = initgroups(user, extra_group);

  if (must_free) free(user);

  if (rc) return env->ThrowErrnoException(errno, "initgroups");

  args.GetReturnValue().Set(0);
}

#endif  // NODE_IMPLEMENTS_POSIX_CREDENTIALS

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  env->SetMethod(target, "safeGetenv", SafeGetenv);

#ifdef NODE_IMPLEMENTS_POSIX_CREDENTIALS
  READONLY_TRUE_PROPERTY(target, "implementsPosixCredentials");
  env->SetMethodNoSideEffect(target, "getuid", GetUid);
  env->SetMethodNoSideEffect(target, "geteuid", GetEUid);
  env->SetMethodNoSideEffect(target, "getgid", GetGid);
  env->SetMethodNoSideEffect(target, "getegid", GetEGid);
  env->SetMethodNoSideEffect(target, "getgroups", GetGroups);

  if (env->owns_process_state()) {
    env->SetMethod(target, "initgroups", InitGroups);
    env->SetMethod(target, "setegid", SetEGid);
    env->SetMethod(target, "seteuid", SetEUid);
    env->SetMethod(target, "setgid", SetGid);
    env->SetMethod(target, "setuid", SetUid);
    env->SetMethod(target, "setgroups", SetGroups);
  }
#endif  // NODE_IMPLEMENTS_POSIX_CREDENTIALS
}

}  // namespace credentials
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(credentials, node::credentials::Initialize)
