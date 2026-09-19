#include "permission/env_permission.h"

#include "env-inl.h"
#include "node_internals.h"
#include "node_mutex.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#if defined(__linux__)
extern char** environ;
#endif

namespace node {

using v8::Array;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::String;
using v8::Value;

namespace permission {

namespace {

constexpr std::string_view kRuntimeEnvironmentDefaults[] = {
    // Read by Node.js itself after startup.
    "NODE_BENCH_CONTEXT",
    "NODE_BENCH_FILE_RUN_ID",
    "NODE_BENCH_RUN_ID",
    "NODE_CHANNEL_FD",
    "NODE_CHANNEL_SERIALIZATION_MODE",
    "NODE_CLUSTER_SCHED_POLICY",
    "NODE_COMPILE_CACHE",
    "NODE_COMPILE_CACHE_PORTABLE",
    "NODE_COMPILE_CACHE_READONLY",
    "NODE_DEBUG",
    "NODE_DEBUG_NATIVE",
    "NODE_DISABLE_COLORS",
    "NODE_DISABLE_COMPILE_CACHE",
    "NODE_EXTRA_CA_CERTS",
    "NODE_ICU_DATA",
    "NODE_INSPECT_RESUME_ON_START",
    "NODE_NO_WARNINGS",
    "NODE_OPTIONS",
    "NODE_PATH",
    "NODE_PENDING_DEPRECATION",
    "NODE_PENDING_PIPE_INSTANCES",
    "NODE_PRESERVE_SYMLINKS",
    "NODE_REDIRECT_WARNINGS",
    "NODE_REPL_EXTERNAL_MODULE",
    "NODE_REPL_HISTORY",
    "NODE_TEST_CONTEXT",
    "NODE_TEST_WORKER_ID",
    "NODE_TLS_REJECT_UNAUTHORIZED",
    "NODE_UNIQUE_ID",
    "NODE_USE_ENV_PROXY",
    "NODE_USE_SYSTEM_CA",
    "NODE_V8_COVERAGE",
    "WATCH_REPORT_DEPENDENCIES",
    // Terminal and CI detection, see lib/internal/tty.js.
    "APPVEYOR",
    "BUILDKITE",
    "CI",
    "CI_NAME",
    "CIRCLECI",
    "COLORTERM",
    "DRONE",
    "FORCE_COLOR",
    "GITEA_ACTIONS",
    "GITHUB_ACTIONS",
    "GITLAB_CI",
    "NO_COLOR",
    "TEAMCITY_VERSION",
    "TERM",
    "TERM_PROGRAM",
    "TERM_PROGRAM_VERSION",
    "TMUX",
    "TRAVIS",
    // libuv.
    "HOME",
    "PATH",
    "TEMP",
    "TMP",
    "TMPDIR",
    "USERPROFILE",
    "UV_THREADPOOL_SIZE",
    "UV_USE_IO_URING",
    // Copied into child process environments by libuv on Windows, and read
    // by lib/child_process.js.
    "COMSPEC",
    "HOMEDRIVE",
    "HOMEPATH",
    "LOGONSERVER",
    "SYSTEMDRIVE",
    "SYSTEMROOT",
    "USERDOMAIN",
    "USERNAME",
    "WINDIR",
    // c-ares and the system resolver.
    "HOSTALIASES",
    "LOCALDOMAIN",
    "RES_OPTIONS",
    // ICU, the time zone and the locale.
    "ICU_DATA",
    "LANG",
    "LANGUAGE",
    "LC_*",
    "TZ",
    // OpenSSL.
    "OPENSSL_CONF",
    "OPENSSL_CONF_INCLUDE",
    "OPENSSL_ENGINES",
    "OPENSSL_MODULES",
    "SSL_CERT_DIR",
    "SSL_CERT_FILE",
    // The dynamic loader, for addons and FFI.
    "DYLD_FALLBACK_LIBRARY_PATH",
    "DYLD_LIBRARY_PATH",
    "LD_LIBRARY_PATH",
    "LIBPATH",
};

struct EnvScrubState {
  Mutex mutex;
  // Whether ScrubProcessEnvironment() removed the variables in `denied`.
  std::atomic<bool> scrubbed{false};
  // Whether RecordAuditedEnvironmentVariables() recorded them instead.
  std::atomic<bool> audited{false};
  std::unordered_set<std::string> denied;
  std::unordered_set<std::string> warned;
};

EnvScrubState& GetScrubState() {
  // Intentionally leaked, so that it can be used during process teardown.
  static EnvScrubState* state = new EnvScrubState();
  return *state;
}

// The key under which a name is stored in the sets above.
std::string NormalizeEnvName(std::string_view name) {
#ifdef _WIN32
  return ToUpper(std::string(name));
#else
  return std::string(name);
#endif
}

bool MatchesAnyPattern(std::span<const std::string> patterns,
                       std::string_view name) {
  return std::any_of(
      patterns.begin(), patterns.end(), [&](const std::string& pattern) {
        return EnvNameMatchesPattern(pattern, name);
      });
}

#ifdef _WIN32
// libuv modifies the environment block of the process, but the C runtime keeps
// its own copy of the environment, which getenv() reads. Removes a variable
// from that copy.
void UnsetCrtEnvironmentVariable(const std::string& name) {
  _wputenv_s(ConvertUTF8ToWideString(name).c_str(), L"");
}
#endif  // _WIN32

#if defined(__linux__)
// Reads the [start, end) address range of the environment block the process
// was started with from fields 50 and 51 of /proc/self/stat (Linux 3.5+).
bool GetInitialEnvironmentBlock(uintptr_t* start, uintptr_t* end) {
  FILE* fp = fopen("/proc/self/stat", "re");
  if (fp == nullptr) return false;
  char buf[4096];
  size_t length = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[length] = '\0';

  // The command name in field 2 may contain spaces and parentheses, so start
  // after the last ')', where field 3 begins.
  char* p = strrchr(buf, ')');
  if (p == nullptr) return false;
  p++;

  constexpr int kEnvStartField = 50;
  constexpr int kEnvEndField = 51;
  for (int field = 3; field <= kEnvEndField; field++) {
    char* next;
    uint64_t value = strtoull(p, &next, 10);
    if (next == p) {
      // A non-numeric field, such as the state in field 3.
      while (*p == ' ') p++;
      while (*p != ' ' && *p != '\0') p++;
      if (*p == '\0') return false;
    } else {
      p = next;
    }
    if (field == kEnvStartField) *start = static_cast<uintptr_t>(value);
    if (field == kEnvEndField) *end = static_cast<uintptr_t>(value);
  }
  return *start != 0 && *start < *end;
}

// Returns the entries for `names` that still point into the initial
// environment block, which unsetenv() leaves untouched.
std::vector<std::pair<char*, size_t>> FindInitialEnvironmentEntries(
    const std::vector<std::string>& names) {
  std::vector<std::pair<char*, size_t>> entries;
  uintptr_t start = 0;
  uintptr_t end = 0;
  if (!GetInitialEnvironmentBlock(&start, &end)) return entries;

  for (char** e = environ; e != nullptr && *e != nullptr; e++) {
    char* entry = *e;
    const uintptr_t address = reinterpret_cast<uintptr_t>(entry);
    if (address < start || address >= end) continue;
    const char* equals = strchr(entry, '=');
    if (equals == nullptr) continue;
    const std::string_view name(entry, equals - entry);
    if (std::find(names.begin(), names.end(), name) == names.end()) continue;
    size_t length = strlen(entry);
    if (address + length > end) length = end - address;
    entries.emplace_back(entry, length);
  }
  return entries;
}
#endif  // defined(__linux__)

}  // namespace

std::vector<std::string> ParseEnvAllowList(
    std::span<const std::string> values) {
  std::vector<std::string> patterns;
  for (const std::string& value : values) {
    size_t begin = 0;
    while (begin <= value.size()) {
      size_t end = value.find(',', begin);
      if (end == std::string::npos) end = value.size();
      if (end > begin) patterns.emplace_back(value.substr(begin, end - begin));
      begin = end + 1;
    }
  }
  return patterns;
}

bool IsValidEnvAllowPattern(std::string_view pattern) {
  const size_t wildcard = pattern.find('*');
  return !pattern.empty() && pattern.find('=') == std::string_view::npos &&
         (wildcard == std::string_view::npos || wildcard == pattern.size() - 1);
}

bool EnvNameMatchesPattern(std::string_view pattern, std::string_view name) {
  if (pattern == "*") return true;
  const bool is_prefix = !pattern.empty() && pattern.back() == '*';
  if (is_prefix) pattern.remove_suffix(1);
  if (is_prefix ? name.size() < pattern.size()
                : name.size() != pattern.size()) {
    return false;
  }
#ifdef _WIN32
  for (size_t i = 0; i < pattern.size(); i++) {
    if (ToUpper(pattern[i]) != ToUpper(name[i])) return false;
  }
  return true;
#else
  return name.compare(0, pattern.size(), pattern) == 0;
#endif
}

// Whether every name `specific` matches is also matched by `general`.
static bool EnvPatternCovers(std::string_view general,
                             std::string_view specific) {
  if (general == "*") return true;
  if (specific.empty() || specific == "*") return false;
  if (specific.back() != '*') return EnvNameMatchesPattern(general, specific);
  // Two prefixes: `P*` only matches names `G*` matches if P starts with G.
  if (general.empty() || general.back() != '*') return false;
  specific.remove_suffix(1);
  return EnvNameMatchesPattern(general, specific);
}

std::vector<std::string> IntersectEnvAllowLists(
    std::span<const std::string> a, std::span<const std::string> b) {
  std::vector<std::string> result;
  for (const std::string& x : a) {
    for (const std::string& y : b) {
      if (EnvPatternCovers(x, y)) {
        result.push_back(y);
      } else if (EnvPatternCovers(y, x)) {
        result.push_back(x);
      }
    }
  }
  return result;
}

std::span<const std::string_view> GetRuntimeEnvironmentDefaults() {
  return kRuntimeEnvironmentDefaults;
}

bool IsRuntimeEnvironmentDefault(std::string_view name) {
  return std::any_of(std::begin(kRuntimeEnvironmentDefaults),
                     std::end(kRuntimeEnvironmentDefaults),
                     [&](std::string_view pattern) {
                       return EnvNameMatchesPattern(pattern, name);
                     });
}

// Returns the names of the variables in the process environment that neither
// `allow` nor, if `keep_runtime_defaults` is set, the runtime defaults match.
// The caller must hold per_process::env_var_mutex.
static std::vector<std::string> FindUnmatchedEnvironmentVariables(
    std::span<const std::string> allow, bool keep_runtime_defaults) {
  uv_env_item_t* items = nullptr;
  int count = 0;
  // Failing to enumerate the environment must not leave it unscrubbed.
  CHECK_EQ(uv_os_environ(&items, &count), 0);
  auto cleanup = OnScopeLeave([&]() { uv_os_free_environ(items, count); });

  std::vector<std::string> names;
  for (int i = 0; i < count; i++) {
    const std::string_view name(items[i].name);
    if (name.empty()) continue;
#ifdef _WIN32
    // Hidden variables, such as the per-drive working directories ("=C:").
    if (name[0] == '=') continue;
#endif
    if (keep_runtime_defaults && IsRuntimeEnvironmentDefault(name)) continue;
    if (MatchesAnyPattern(allow, name)) continue;
    names.emplace_back(name);
  }
  return names;
}

std::vector<std::string> FindDeniedEnvironmentVariables(
    std::span<const std::string> allow) {
  Mutex::ScopedLock env_lock(per_process::env_var_mutex);
  return FindUnmatchedEnvironmentVariables(allow, true);
}

std::vector<std::string> ScrubProcessEnvironment(
    const EnvScrubOptions& options) {
  EnvScrubState& state = GetScrubState();
  Mutex::ScopedLock state_lock(state.mutex);
  Mutex::ScopedLock env_lock(per_process::env_var_mutex);

  std::vector<std::string> removed = FindUnmatchedEnvironmentVariables(
      options.allow, options.keep_runtime_defaults);

#if defined(__linux__)
  std::vector<std::pair<char*, size_t>> initial_entries;
  if (options.wipe_initial_block) {
    initial_entries = FindInitialEnvironmentEntries(removed);
  }
#endif

  for (const std::string& name : removed) {
    uv_os_unsetenv(name.c_str());
#ifdef _WIN32
    UnsetCrtEnvironmentVariable(name);
#endif
    state.denied.insert(NormalizeEnvName(name));
  }

#if defined(__linux__)
  // Now that environ no longer refers to them, overwrite the removed entries
  // so that /proc/<pid>/environ does not expose them either.
  for (const auto& [entry, length] : initial_entries) {
    memset(entry, 0, length);
  }
#endif

  state.scrubbed.store(true);
  return removed;
}

void RecordAuditedEnvironmentVariables(std::span<const std::string> allow) {
  EnvScrubState& state = GetScrubState();
  Mutex::ScopedLock state_lock(state.mutex);
  Mutex::ScopedLock env_lock(per_process::env_var_mutex);
  for (const std::string& name :
       FindUnmatchedEnvironmentVariables(allow, true)) {
    state.denied.insert(NormalizeEnvName(name));
  }
  state.audited.store(true);
}

bool IsProcessEnvironmentScrubbed() {
  return GetScrubState().scrubbed.load();
}

bool WasRemovedByEnvironmentScrub(std::string_view name) {
  EnvScrubState& state = GetScrubState();
  if (!state.scrubbed.load()) return false;
  Mutex::ScopedLock lock(state.mutex);
  return state.denied.contains(NormalizeEnvName(name));
}

bool WasDeniedAtStartup(std::string_view name) {
  EnvScrubState& state = GetScrubState();
  if (!state.scrubbed.load() && !state.audited.load()) return false;
  Mutex::ScopedLock lock(state.mutex);
  return state.denied.contains(NormalizeEnvName(name));
}

bool ShouldWarnAboutRemovedEnvVar(std::string_view name) {
  EnvScrubState& state = GetScrubState();
  Mutex::ScopedLock lock(state.mutex);
  return state.warned.insert(NormalizeEnvName(name)).second;
}

void EnvPermission::Apply(Environment* env,
                          std::span<const std::string> allow,
                          PermissionScope scope) {
  patterns_.assign(allow.begin(), allow.end());
  if (std::find(patterns_.begin(), patterns_.end(), "*") != patterns_.end()) {
    granted_all_.store(true);
  }
}

void EnvPermission::Drop(Environment* env,
                         PermissionScope scope,
                         std::string_view param) {
  granted_all_.store(false);
  if (param.empty()) {
    dropped_all_ = true;
    patterns_.clear();
    dropped_.clear();
  } else {
    dropped_.insert(NormalizeEnvName(param));
  }
  RemoveFromEnvironment(env, param.empty(), param);
}

bool EnvPermission::is_granted(Environment* env,
                               PermissionScope perm,
                               std::string_view param) const {
  if (param.empty()) return granted_all();
  if (!dropped_.empty() && dropped_.contains(NormalizeEnvName(param))) {
    return false;
  }
  if (granted_all()) return true;
  return IsRuntimeEnvironmentDefault(param) ||
         (!dropped_all_ && MatchesAnyPattern(patterns_, param));
}

void EnvPermission::RemoveFromEnvironment(Environment* env,
                                          bool drop_all,
                                          std::string_view name) {
  if (env == nullptr) return;
  // An Environment that does not own the process state shares the real
  // process environment with the embedder, and must not modify it.
  if (env->env_vars() == per_process::system_environment &&
      !env->owns_process_state()) {
    return;
  }

  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();

  const auto remove = [&](Local<String> key) {
    env->env_vars()->Delete(isolate, key);
#ifdef _WIN32
    if (env->env_vars() == per_process::system_environment) {
      UnsetCrtEnvironmentVariable(Utf8Value(isolate, key).ToString());
    }
#endif
  };

  if (!drop_all) {
    Local<Value> key;
    if (ToV8Value(context, name, isolate).ToLocal(&key) && key->IsString()) {
      remove(key.As<String>());
    }
    return;
  }

  Local<Array> keys;
  if (!env->env_vars()->Enumerate(isolate).ToLocal(&keys)) return;
  const uint32_t length = keys->Length();
  for (uint32_t i = 0; i < length; i++) {
    Local<Value> key;
    if (!keys->Get(context, i).ToLocal(&key) || !key->IsString()) continue;
    Utf8Value key_utf8(isolate, key);
    if (IsRuntimeEnvironmentDefault(key_utf8.ToStringView())) continue;
    remove(key.As<String>());
  }
}

}  // namespace permission

}  // namespace node
