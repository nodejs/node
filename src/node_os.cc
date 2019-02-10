// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "env-inl.h"
#include "string_bytes.h"
#include "util.h"

#include <array>
#include <errno.h>
#include <string.h>

#ifdef __MINGW32__
# include <io.h>
#endif  // __MINGW32__

#ifdef __POSIX__
# include <limits.h>        // PATH_MAX on Solaris.
# include <unistd.h>        // gethostname, sysconf
#endif  // __POSIX__

namespace node {
namespace os {

using v8::Array;
using v8::ArrayBuffer;
using v8::Boolean;
using v8::Context;
using v8::Float64Array;
using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;


static void GetHostname(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  char buf[UV_MAXHOSTNAMESIZE];
  size_t size = sizeof(buf);
  int r = uv_os_gethostname(buf, &size);

  if (r != 0) {
    CHECK_GE(args.Length(), 1);
    env->CollectUVExceptionInfo(args[args.Length() - 1], r,
                                "uv_os_gethostname");
    return args.GetReturnValue().SetUndefined();
  }

  args.GetReturnValue().Set(OneByteString(env->isolate(), buf));
}


static void GetOSType(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uv_utsname_t info;
  int err = uv_os_uname(&info);

  if (err != 0) {
    CHECK_GE(args.Length(), 1);
    env->CollectUVExceptionInfo(args[args.Length() - 1], err, "uv_os_uname");
    return args.GetReturnValue().SetUndefined();
  }

  args.GetReturnValue().Set(OneByteString(env->isolate(), info.sysname));
}


static void GetOSRelease(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uv_utsname_t info;
  int err = uv_os_uname(&info);

  if (err != 0) {
    CHECK_GE(args.Length(), 1);
    env->CollectUVExceptionInfo(args[args.Length() - 1], err, "uv_os_uname");
    return args.GetReturnValue().SetUndefined();
  }

  args.GetReturnValue().Set(OneByteString(env->isolate(), info.release));
}


static void GetCPUInfo(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  uv_cpu_info_t* cpu_infos;
  int count;

  int err = uv_cpu_info(&cpu_infos, &count);
  if (err)
    return;

  // It's faster to create an array packed with all the data and
  // assemble them into objects in JS than to call Object::Set() repeatedly
  // The array is in the format
  // [model, speed, (5 entries of cpu_times), model2, speed2, ...]
  std::vector<Local<Value>> result(count * 7);
  for (int i = 0; i < count; i++) {
    uv_cpu_info_t* ci = cpu_infos + i;
    result[i * 7] = OneByteString(isolate, ci->model);
    result[i * 7 + 1] = Number::New(isolate, ci->speed);
    result[i * 7 + 2] = Number::New(isolate, ci->cpu_times.user);
    result[i * 7 + 3] = Number::New(isolate, ci->cpu_times.nice);
    result[i * 7 + 4] = Number::New(isolate, ci->cpu_times.sys);
    result[i * 7 + 5] = Number::New(isolate, ci->cpu_times.idle);
    result[i * 7 + 6] = Number::New(isolate, ci->cpu_times.irq);
  }

  uv_free_cpu_info(cpu_infos, count);
  args.GetReturnValue().Set(Array::New(isolate, result.data(), result.size()));
}


static void GetFreeMemory(const FunctionCallbackInfo<Value>& args) {
  double amount = uv_get_free_memory();
  if (amount < 0)
    return;
  args.GetReturnValue().Set(amount);
}


static void GetTotalMemory(const FunctionCallbackInfo<Value>& args) {
  double amount = uv_get_total_memory();
  if (amount < 0)
    return;
  args.GetReturnValue().Set(amount);
}


static void GetUptime(const FunctionCallbackInfo<Value>& args) {
  double uptime;
  int err = uv_uptime(&uptime);
  if (err == 0)
    args.GetReturnValue().Set(uptime);
}


static void GetLoadAvg(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsFloat64Array());
  Local<Float64Array> array = args[0].As<Float64Array>();
  CHECK_EQ(array->Length(), 3);
  Local<ArrayBuffer> ab = array->Buffer();
  double* loadavg = static_cast<double*>(ab->GetContents().Data());
  uv_loadavg(loadavg);
}


static void GetInterfaceAddresses(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  uv_interface_address_t* interfaces;
  int count, i;
  char ip[INET6_ADDRSTRLEN];
  char netmask[INET6_ADDRSTRLEN];
  std::array<char, 18> mac;
  Local<String> name, family;

  int err = uv_interface_addresses(&interfaces, &count);

  if (err == UV_ENOSYS)
    return args.GetReturnValue().SetUndefined();

  if (err) {
    CHECK_GE(args.Length(), 1);
    env->CollectUVExceptionInfo(args[args.Length() - 1], errno,
                                "uv_interface_addresses");
    return args.GetReturnValue().SetUndefined();
  }

  Local<Value> no_scope_id = Integer::New(isolate, -1);
  std::vector<Local<Value>> result(count * 7);
  for (i = 0; i < count; i++) {
    const char* const raw_name = interfaces[i].name;

    // Use UTF-8 on both Windows and Unixes (While it may be true that UNIX
    // systems are somewhat encoding-agnostic here, it’s more than reasonable
    // to assume UTF8 as the default as well. It’s what people will expect if
    // they name the interface from any input that uses UTF-8, which should be
    // the most frequent case by far these days.)
    name = String::NewFromUtf8(isolate, raw_name,
        v8::NewStringType::kNormal).ToLocalChecked();

    snprintf(mac.data(),
             mac.size(),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             static_cast<unsigned char>(interfaces[i].phys_addr[0]),
             static_cast<unsigned char>(interfaces[i].phys_addr[1]),
             static_cast<unsigned char>(interfaces[i].phys_addr[2]),
             static_cast<unsigned char>(interfaces[i].phys_addr[3]),
             static_cast<unsigned char>(interfaces[i].phys_addr[4]),
             static_cast<unsigned char>(interfaces[i].phys_addr[5]));

    if (interfaces[i].address.address4.sin_family == AF_INET) {
      uv_ip4_name(&interfaces[i].address.address4, ip, sizeof(ip));
      uv_ip4_name(&interfaces[i].netmask.netmask4, netmask, sizeof(netmask));
      family = env->ipv4_string();
    } else if (interfaces[i].address.address4.sin_family == AF_INET6) {
      uv_ip6_name(&interfaces[i].address.address6, ip, sizeof(ip));
      uv_ip6_name(&interfaces[i].netmask.netmask6, netmask, sizeof(netmask));
      family = env->ipv6_string();
    } else {
      strncpy(ip, "<unknown sa family>", INET6_ADDRSTRLEN);
      family = env->unknown_string();
    }

    result[i * 7] = name;
    result[i * 7 + 1] = OneByteString(isolate, ip);
    result[i * 7 + 2] = OneByteString(isolate, netmask);
    result[i * 7 + 3] = family;
    result[i * 7 + 4] = FIXED_ONE_BYTE_STRING(isolate, mac);
    result[i * 7 + 5] =
      interfaces[i].is_internal ? True(isolate) : False(isolate);
    if (interfaces[i].address.address4.sin_family == AF_INET6) {
      uint32_t scopeid = interfaces[i].address.address6.sin6_scope_id;
      result[i * 7 + 6] = Integer::NewFromUnsigned(isolate, scopeid);
    } else {
      result[i * 7 + 6] = no_scope_id;
    }
  }

  uv_free_interface_addresses(interfaces, count);
  args.GetReturnValue().Set(Array::New(isolate, result.data(), result.size()));
}


static void GetHomeDirectory(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  char buf[PATH_MAX];

  size_t len = sizeof(buf);
  const int err = uv_os_homedir(buf, &len);

  if (err) {
    CHECK_GE(args.Length(), 1);
    env->CollectUVExceptionInfo(args[args.Length() - 1], err, "uv_os_homedir");
    return args.GetReturnValue().SetUndefined();
  }

  Local<String> home = String::NewFromUtf8(env->isolate(),
                                           buf,
                                           v8::NewStringType::kNormal,
                                           len).ToLocalChecked();
  args.GetReturnValue().Set(home);
}


static void GetUserInfo(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uv_passwd_t pwd;
  enum encoding encoding;

  if (args[0]->IsObject()) {
    Local<Object> options = args[0].As<Object>();
    MaybeLocal<Value> maybe_encoding = options->Get(env->context(),
                                                    env->encoding_string());
    if (maybe_encoding.IsEmpty())
      return;

    Local<Value> encoding_opt = maybe_encoding.ToLocalChecked();
    encoding = ParseEncoding(env->isolate(), encoding_opt, UTF8);
  } else {
    encoding = UTF8;
  }

  const int err = uv_os_get_passwd(&pwd);

  if (err) {
    CHECK_GE(args.Length(), 2);
    env->CollectUVExceptionInfo(args[args.Length() - 1], err,
                                "uv_os_get_passwd");
    return args.GetReturnValue().SetUndefined();
  }

  OnScopeLeave free_passwd([&]() { uv_os_free_passwd(&pwd); });

  Local<Value> error;

  Local<Value> uid = Number::New(env->isolate(), pwd.uid);
  Local<Value> gid = Number::New(env->isolate(), pwd.gid);
  MaybeLocal<Value> username = StringBytes::Encode(env->isolate(),
                                                   pwd.username,
                                                   encoding,
                                                   &error);
  MaybeLocal<Value> homedir = StringBytes::Encode(env->isolate(),
                                                  pwd.homedir,
                                                  encoding,
                                                  &error);
  MaybeLocal<Value> shell;

  if (pwd.shell == nullptr)
    shell = Null(env->isolate());
  else
    shell = StringBytes::Encode(env->isolate(), pwd.shell, encoding, &error);

  if (username.IsEmpty() || homedir.IsEmpty() || shell.IsEmpty()) {
    CHECK(!error.IsEmpty());
    env->isolate()->ThrowException(error);
    return;
  }

  Local<Object> entry = Object::New(env->isolate());

  entry->Set(env->context(), env->uid_string(), uid).FromJust();
  entry->Set(env->context(), env->gid_string(), gid).FromJust();
  entry->Set(env->context(),
             env->username_string(),
             username.ToLocalChecked()).FromJust();
  entry->Set(env->context(),
             env->homedir_string(),
             homedir.ToLocalChecked()).FromJust();
  entry->Set(env->context(),
             env->shell_string(),
             shell.ToLocalChecked()).FromJust();

  args.GetReturnValue().Set(entry);
}


static void SetPriority(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 3);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsInt32());

  const int pid = args[0].As<Int32>()->Value();
  const int priority = args[1].As<Int32>()->Value();
  const int err = uv_os_setpriority(pid, priority);

  if (err) {
    CHECK(args[2]->IsObject());
    env->CollectUVExceptionInfo(args[2], err, "uv_os_setpriority");
  }

  args.GetReturnValue().Set(err);
}


static void GetPriority(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 2);
  CHECK(args[0]->IsInt32());

  const int pid = args[0].As<Int32>()->Value();
  int priority;
  const int err = uv_os_getpriority(pid, &priority);

  if (err) {
    CHECK(args[1]->IsObject());
    env->CollectUVExceptionInfo(args[1], err, "uv_os_getpriority");
    return;
  }

  args.GetReturnValue().Set(priority);
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "getHostname", GetHostname);
  env->SetMethod(target, "getLoadAvg", GetLoadAvg);
  env->SetMethod(target, "getUptime", GetUptime);
  env->SetMethod(target, "getTotalMem", GetTotalMemory);
  env->SetMethod(target, "getFreeMem", GetFreeMemory);
  env->SetMethod(target, "getCPUs", GetCPUInfo);
  env->SetMethod(target, "getOSType", GetOSType);
  env->SetMethod(target, "getOSRelease", GetOSRelease);
  env->SetMethod(target, "getInterfaceAddresses", GetInterfaceAddresses);
  env->SetMethod(target, "getHomeDirectory", GetHomeDirectory);
  env->SetMethod(target, "getUserInfo", GetUserInfo);
  env->SetMethod(target, "setPriority", SetPriority);
  env->SetMethod(target, "getPriority", GetPriority);
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "isBigEndian"),
              Boolean::New(env->isolate(), IsBigEndian())).FromJust();
}

}  // namespace os
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(os, node::os::Initialize)
