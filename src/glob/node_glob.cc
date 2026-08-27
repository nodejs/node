#include "glob/node_glob.h"

#include <cmath>
#include <limits>

#include "env-inl.h"
#include "glob/glob_matcher.h"
#include "glob/glob_parser.h"
#include "glob/glob_walker.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util-inl.h"
#include "v8.h"

namespace node::glob {

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::Promise;
using v8::SnapshotCreator;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

namespace {

constexpr uint32_t kFlagWindows = 1;
constexpr uint32_t kFlagNocase = 2;
constexpr uint32_t kFlagDot = 4;
constexpr uint32_t kFlagFollowSymlinks = 8;
constexpr uint32_t kFlagWithFileTypes = 16;
constexpr uint32_t kFlagMatchBase = 32;
// Not sent from JavaScript, only distinguishes cache keys
constexpr uint32_t kFlagNocaseLiterals = 64;

// The host's own facts are not sent from JavaScript. Matching keeps the
// caller's `windows` choice (path.win32/posix work from anywhere), so only
// case sensitivity is the host's; a walk always follows the host's rules.
#ifdef _WIN32
constexpr uint32_t kHostMatchFlags = kFlagNocase;
constexpr uint32_t kHostWalkFlags = kFlagWindows | kFlagNocase;
#elif defined(__APPLE__)
constexpr uint32_t kHostMatchFlags = kFlagNocase;
constexpr uint32_t kHostWalkFlags = kFlagNocase;
#else
constexpr uint32_t kHostMatchFlags = 0;
constexpr uint32_t kHostWalkFlags = 0;
#endif

CompileFlags UnpackFlags(uint32_t packed) {
  CompileFlags flags;
  flags.windows = (packed & kFlagWindows) != 0;
  flags.nocase = (packed & kFlagNocase) != 0;
  flags.dot = (packed & kFlagDot) != 0;
  flags.match_base = (packed & kFlagMatchBase) != 0;
  return flags;
}

// Builds the compiled-pattern cache key into `key` (for caching)
void BuildCacheKey(PatternView pattern,
                   const CompileFlags& flags,
                   PatternString* key) {
  const uint32_t packed =
      (flags.windows ? kFlagWindows : 0) | (flags.nocase ? kFlagNocase : 0) |
      (flags.dot ? kFlagDot : 0) | (flags.match_base ? kFlagMatchBase : 0) |
      (flags.nocase_magic_only ? 0 : kFlagNocaseLiterals);
  key->assign(1, static_cast<char16_t>(packed));
  key->append(pattern);
}

// Patterns that do not compile throw, try a different pattern
void ThrowCompileError(Isolate* isolate, CompileError error) {
  const bool bad_regexp = error == CompileError::kInvalidRegExp;
  const char* message;
  switch (error) {
    case CompileError::kPatternTooDeep:
      message = "pattern nests too deeply";
      break;
    case CompileError::kInvalidRegExp:
      message = "invalid generated regular expression";
      break;
    default:
      message = "pattern is too long";
      break;
  }
  Local<String> text = OneByteString(isolate, message);
  isolate->ThrowException(bad_regexp ? Exception::SyntaxError(text)
                                     : Exception::TypeError(text));
}

constexpr size_t kMarshalStackEntries = GlobRequest::kBatchSize;

// Builds [paths array, types array|undefined] for a batch of results.
bool MarshalEntries(Environment* env,
                    const std::vector<WalkEntry>& entries,
                    bool with_types,
                    Local<Array>* paths_out,
                    Local<Value>* types_out) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  MaybeStackBuffer<Value, kMarshalStackEntries> paths(isolate, entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    if (!ToV8Value(context, entries[i].path, isolate).ToLocal(&paths[i])) {
      return false;
    }
  }
  *paths_out = paths.ToArray();
  *types_out = Undefined(isolate);
  if (with_types) {
    MaybeStackBuffer<Value, kMarshalStackEntries> types(isolate,
                                                        entries.size());
    for (size_t i = 0; i < entries.size(); i++) {
      types[i] = Integer::New(isolate, entries[i].type);
    }
    *types_out = types.ToArray();
  }
  return true;
}

size_t ProgramSize(const CompiledPattern& p) {
  size_t total = sizeof(CompiledPattern);
  for (const CompiledPattern::Row& row : p.rows) {
    total += sizeof(CompiledPattern::Row);
    for (const PartMatcher& part : row.parts) {
      total += sizeof(PartMatcher) + part.literal.size() * sizeof(char16_t) +
               part.program.literal.size() * sizeof(char16_t) +
               part.program.nodes.size() * sizeof(RxNode);
      for (const RxNode& node : part.program.nodes) {
        total += node.text.size() * sizeof(char16_t) +
                 (node.klass.positive.ranges().size() +
                  node.klass.negative.ranges().size()) *
                     sizeof(CharSet::Range) +
                 node.children.size() * sizeof(uint32_t);
      }
    }
  }
  return total;
}

}  // namespace

BindingData::BindingData(Realm* realm,
                         Local<Object> object,
                         InternalFieldInfoBase* info)
    : SnapshotableObject(realm, object, type_int) {}

bool BindingData::PrepareForSerialization(Local<Context> context,
                                          SnapshotCreator* creator) {
  cache_.Clear();
  return true;
}

InternalFieldInfoBase* BindingData::Serialize(int index) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  return InternalFieldInfoBase::New<InternalFieldInfo>(type());
}

void BindingData::Deserialize(Local<Context> context,
                              Local<Object> holder,
                              int index,
                              InternalFieldInfoBase* info) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  Realm* realm = Realm::GetCurrent(context);
  HandleScope scope(realm->isolate());
  BindingData* binding = realm->AddBindingData<BindingData>(holder, info);
  CHECK_NOT_NULL(binding);
}

void BindingData::MemoryInfo(MemoryTracker* tracker) const {
  size_t bytes = 0;
  for (const auto& [key, entry] : cache_) {
    bytes += key.size() * sizeof(char16_t) + sizeof(CacheEntry) +
             ProgramSize(entry->compiled);
  }
  tracker->TrackFieldWithSize("compiled_pattern_cache", bytes);
}

CompiledPatternPtr BindingData::Lookup(PatternView pattern,
                                       const CompileFlags& flags,
                                       CompileError* error) {
  BuildCacheKey(pattern, flags, &lookup_key_);
  const PatternString& key = lookup_key_;
  std::shared_ptr<CacheEntry> entry;
  if (std::shared_ptr<CacheEntry>* found = cache_.GetIf(key)) {
    entry = *found;
  } else {
    entry = std::make_shared<CacheEntry>();
    ParseResult parsed = ParsePattern(pattern, flags);
    if (parsed.error != CompileError::kNone) {
      entry->error = parsed.error;
    } else {
      entry->compiled = CompileProgram(std::move(parsed.pattern), flags);
      entry->error = entry->compiled.error;
    }
    cache_.Put(key, entry);
  }
  *error = entry->error;
  if (entry->error != CompileError::kNone) return nullptr;
  return CompiledPatternPtr(entry, &entry->compiled);
}

void BindingData::MatchesGlob(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  BindingData* binding = realm->GetBindingData<BindingData>();
  Isolate* isolate = realm->isolate();

  CHECK_EQ(args.Length(), 3);
  CHECK(args[0]->IsString());  // path
  CHECK(args[1]->IsString());  // pattern
  CHECK(args[2]->IsUint32());  // packed flags

  const CompileFlags flags =
      UnpackFlags(args[2].As<Uint32>()->Value() | kHostMatchFlags);

  TwoByteValue pattern(isolate, args[1]);
  CompileError error = CompileError::kNone;
  const CompiledPatternPtr compiled =
      binding->Lookup(pattern.ToU16StringView(), flags, &error);
  if (compiled == nullptr) {
    ThrowCompileError(isolate, error);
    return;
  }

  TwoByteValue path(isolate, args[0]);
  args.GetReturnValue().Set(MatchPattern(*compiled, path.ToU16StringView()));
}

// Whether the pattern is anything more than a simple, literal path
// (Basically, is it a glob or just a normal string)
void BindingData::HasMagic(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  BindingData* binding = realm->GetBindingData<BindingData>();
  Isolate* isolate = realm->isolate();

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  const CompileFlags flags = UnpackFlags(kHostWalkFlags);

  TwoByteValue pattern(isolate, args[0]);
  CompileError error = CompileError::kNone;
  const CompiledPatternPtr compiled =
      binding->Lookup(pattern.ToU16StringView(), flags, &error);
  if (compiled == nullptr) {
    ThrowCompileError(isolate, error);
    return;
  }
  for (const CompiledPattern::Row& row : compiled->rows) {
    for (const PartMatcher& part : row.parts) {
      if (part.kind != PartMatcher::Kind::kLiteral) {
        args.GetReturnValue().Set(true);
        return;
      }
    }
  }
  args.GetReturnValue().Set(false);
}

// Both strings are one-byte and the pattern is normally
// already compiled, so the whole call is a hash lookup plus the match.
// Reads (cwd, includePatterns[], excludePatterns[], flags, excludeFn,
// maxDepth) and compiles the patterns.
bool BindingData::ReadWalkArguments(const FunctionCallbackInfo<Value>& args,
                                    WalkOptions* options,
                                    std::vector<CompiledPatternPtr>* includes,
                                    std::vector<CompiledPatternPtr>* excludes,
                                    std::unique_ptr<JsExcludeFilter>* filter) {
  Realm* realm = Realm::GetCurrent(args);
  Isolate* isolate = realm->isolate();
  Local<Context> context = realm->context();

  CHECK_EQ(args.Length(), 6);
  CHECK(args[0]->IsString());                              // cwd
  CHECK(args[1]->IsArray());                               // include patterns
  CHECK(args[2]->IsArray());                               // exclude patterns
  CHECK(args[3]->IsUint32());                              // packed flags
  CHECK(args[4]->IsFunction() || args[4]->IsUndefined());  // exclude callback
  CHECK(args[5]->IsNumber());                              // maxDepth

  const uint32_t packed = args[3].As<Uint32>()->Value() | kHostWalkFlags;
  const CompileFlags flags = UnpackFlags(packed);
  // Exclude matching is a pure string comparison with no filesystem lookups,
  // so unlike include patterns, literal segments must also match
  // case-insensitively on case-insensitive platforms.
  CompileFlags exclude_flags = flags;
  exclude_flags.nocase_magic_only = false;

  options->follow_symlinks = (packed & kFlagFollowSymlinks) != 0;
  options->with_file_types = (packed & kFlagWithFileTypes) != 0;
  if (args[4]->IsFunction()) {
    *filter =
        std::make_unique<JsExcludeFilter>(realm->env(), args[4].As<Function>());
    options->exclude_filter = filter->get();
  }
  {
    Utf8Value cwd(isolate, args[0]);
    options->cwd = cwd.ToString();
  }
  // A non-negative integer, or Infinity for no limit, validated in JS.
  const double max_depth = args[5].As<Number>()->Value();
  if (std::isfinite(max_depth)) {
    CHECK_GE(max_depth, 0);
    options->max_depth = static_cast<size_t>(max_depth);
  }

  for (int which = 0; which < 2; which++) {
    std::vector<CompiledPatternPtr>* out = which == 0 ? includes : excludes;
    Local<Array> list = args[1 + which].As<Array>();
    const uint32_t length = list->Length();
    out->reserve(length);
    for (uint32_t i = 0; i < length; i++) {
      Local<Value> value;
      if (!list->Get(context, i).ToLocal(&value)) return false;
      CHECK(value->IsString());
      TwoByteValue pattern(isolate, value);
      CompileError error = CompileError::kNone;
      CompiledPatternPtr compiled = Lookup(pattern.ToU16StringView(),
                                           which == 0 ? flags : exclude_flags,
                                           &error);
      if (compiled == nullptr) {
        ThrowCompileError(isolate, error);
        return false;
      }
      out->push_back(std::move(compiled));
    }
  }
  return true;
}

// globSync(cwd, includePatterns[], excludePatterns[], flags, excludeFn,
// maxDepth)
void BindingData::GlobSync(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  Environment* env = realm->env();
  BindingData* binding = realm->GetBindingData<BindingData>();
  Isolate* isolate = realm->isolate();

  WalkOptions options;
  std::vector<CompiledPatternPtr> includes;
  std::vector<CompiledPatternPtr> excludes;
  std::unique_ptr<JsExcludeFilter> filter;
  if (!binding->ReadWalkArguments(
          args, &options, &includes, &excludes, &filter)) {
    return;
  }

  std::vector<WalkEntry> entries;
  glob::GlobSync(env, options, includes, excludes, &entries);
  // An exclude callback that threw leaves its exception pending.
  if (filter != nullptr && filter->failed()) return;

  Local<Array> paths;
  Local<Value> types;
  if (!MarshalEntries(env, entries, options.with_file_types, &paths, &types)) {
    return;
  }
  if (!options.with_file_types) {
    args.GetReturnValue().Set(paths);
    return;
  }
  Local<Value> pair[] = {paths, types};
  args.GetReturnValue().Set(Array::New(isolate, pair, arraysize(pair)));
}

// Calls the user's `exclude`
JsExcludeFilter::JsExcludeFilter(Environment* env, Local<Function> callback)
    : env_(env), callback_(env->isolate(), callback) {}

Maybe<bool> JsExcludeFilter::Call(bool entry,
                                  std::string_view first,
                                  std::string_view second,
                                  int type) {
  Isolate* isolate = env_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = env_->context();
  // (isEntry, name-or-path, parentPath, type), see #excludeAdapter().
  Local<Value> argv[4];
  if (!ToV8Value(context, first, isolate).ToLocal(&argv[1]) ||
      !ToV8Value(context, second, isolate).ToLocal(&argv[2])) {
    return Nothing<bool>();
  }
  argv[0] = Boolean::New(isolate, entry);
  argv[3] = Integer::New(isolate, type);
  Local<Value> result;
  if (!callback_.Get(isolate)
           ->Call(context, Undefined(isolate), arraysize(argv), argv)
           .ToLocal(&result)) {
    return Nothing<bool>();
  }
  return Just(result->BooleanValue(isolate));
}

bool JsExcludeFilter::ExcludesPath(std::string_view path) {
  if (failed_) return false;
  bool excluded = false;
  failed_ = !Call(false, path, std::string_view(), 0).To(&excluded);
  return excluded;
}

bool JsExcludeFilter::ExcludesEntry(std::string_view name,
                                    std::string_view parent_path,
                                    int type) {
  if (failed_) return false;
  bool excluded = false;
  failed_ = !Call(true, name, parent_path, type).To(&excluded);
  return excluded;
}

GlobRequest::GlobRequest(Environment* env,
                         Local<Object> object,
                         const WalkOptions& options,
                         const std::vector<CompiledPatternPtr>& includes,
                         const std::vector<CompiledPatternPtr>& excludes,
                         std::unique_ptr<JsExcludeFilter> filter)
    : AsyncWrap(env, object, AsyncWrap::PROVIDER_GLOBREQUEST),
      ThreadPoolWork(env, "glob"),
      filter_(std::move(filter)),
      walk_(env, options, includes, excludes),
      with_file_types_(options.with_file_types),
      inline_only_(env->permission()->enabled() || filter_ != nullptr) {
  MakeWeak();
}

void GlobRequest::MemoryInfo(MemoryTracker* tracker) const {
  // A slice in flight owns batch_ on another thread.
  if (!in_flight_) {
    tracker->TrackFieldWithSize("batch", batch_.size() * sizeof(WalkEntry));
  }
}

void GlobRequest::DoThreadPoolWork() {
  const size_t max_results =
      drain_ ? std::numeric_limits<size_t>::max() : kBatchSize;
  done_ = walk_.RunSlice(max_results, inline_only_, &batch_);
}

// Builds [paths, types|undefined, done]
MaybeLocal<Value> GlobRequest::Settle() {
  Environment* env = AsyncWrap::env();
  Isolate* isolate = env->isolate();

  Local<Array> paths;
  Local<Value> types;
  if (!MarshalEntries(env, batch_, with_file_types_, &paths, &types)) {
    return MaybeLocal<Value>();
  }
  batch_.clear();
  Local<Value> result[] = {paths, types, Boolean::New(isolate, done_)};
  return Array::New(isolate, result, arraysize(result));
}

void GlobRequest::AfterThreadPoolWork(int status) {
  in_flight_ = false;
  // Environment teardown or a cancelled queue entry must not call back
  // into JavaScript.
  if (status == UV_ECANCELED || !AsyncWrap::env()->can_call_into_js()) {
    resolver_.Reset();
    done_ = true;
    walk_.Stop();
    MakeWeak();
    return;
  }
  if (cancelled_) done_ = true;
  // An exclude callback that threw leaves its exception pending
  if (filter_ != nullptr && filter_->failed()) {
    resolver_.Reset();
    done_ = true;
    walk_.Stop();
    return;
  }
  // A finished walk keeps no threads waiting for garbage collection.
  if (done_) walk_.Stop();
  Environment* env = AsyncWrap::env();
  HandleScope scope(env->isolate());
  Local<Context> context = env->context();
  Context::Scope context_scope(context);
  InternalCallbackScope callback_scope(this);
  Local<Promise::Resolver> resolver = resolver_.Get(env->isolate());
  resolver_.Reset();
  // The request is idle again, so it may be collected with its handle.
  MakeWeak();

  Local<Value> value;
  if (!Settle().ToLocal(&value)) return;
  USE(resolver->Resolve(context, value));
}

// next() -> Promise<[paths, types, done]>: one batch of results.
void GlobRequest::Next(const FunctionCallbackInfo<Value>& args) {
  Pull(args, /*drain=*/false);
}

// all() -> Promise<[paths, types, done]>: the rest of the walk at once.
void GlobRequest::All(const FunctionCallbackInfo<Value>& args) {
  Pull(args, /*drain=*/true);
}

void GlobRequest::Pull(const FunctionCallbackInfo<Value>& args, bool drain) {
  GlobRequest* request;
  ASSIGN_OR_RETURN_UNWRAP(&request, args.This());
  Environment* env = request->AsyncWrap::env();
  Isolate* isolate = env->isolate();

  // Reachable through the async_hooks resource object, so misuse (a
  // second pull, or a pull from inside an exclude callback) must throw,
  // not crash or clobber the pending resolver.
  if (request->in_flight_) {
    THROW_ERR_INVALID_STATE(env, "glob is already waiting for results");
    return;
  }
  request->drain_ = drain;

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env->context()).ToLocal(&resolver)) return;
  args.GetReturnValue().Set(resolver->GetPromise());

  if (request->cancelled_ || request->done_) {
    request->done_ = true;
    Local<Value> value;
    if (!request->Settle().ToLocal(&value)) return;
    USE(resolver->Resolve(env->context(), value));
    return;
  }

  request->resolver_.Reset(isolate, resolver);
  request->in_flight_ = true;
  if (request->inline_only_) {
    request->DoThreadPoolWork();
    request->AfterThreadPoolWork(0);
    return;
  }
  // Keep the request alive for as long as the worker holds a pointer to it.
  request->ClearWeak();
  request->ScheduleWork();
}

// Stops the walk. A slice in flight still owns done_; the cancellation
// takes effect when it completes, but the walk's threads stop now.
void GlobRequest::Cancel(const FunctionCallbackInfo<Value>& args) {
  GlobRequest* request;
  ASSIGN_OR_RETURN_UNWRAP(&request, args.This());
  request->cancelled_ = true;
  if (!request->in_flight_) request->done_ = true;
  request->walk_.Stop();
}

void BindingData::GlobStart(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  Environment* env = realm->env();
  BindingData* binding = realm->GetBindingData<BindingData>();

  WalkOptions options;
  std::vector<CompiledPatternPtr> includes;
  std::vector<CompiledPatternPtr> excludes;
  std::unique_ptr<JsExcludeFilter> filter;
  if (!binding->ReadWalkArguments(
          args, &options, &includes, &excludes, &filter)) {
    return;
  }

  Local<Object> object;
  if (!env->glob_request_template()
           ->InstanceTemplate()
           ->NewInstance(realm->context())
           .ToLocal(&object)) {
    return;
  }
  new GlobRequest(env, object, options, includes, excludes, std::move(filter));
  args.GetReturnValue().Set(object);
}

void BindingData::CreatePerIsolateProperties(IsolateData* isolate_data,
                                             Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  SetMethodNoSideEffect(isolate, target, "matchesGlob", MatchesGlob);
  SetMethodNoSideEffect(isolate, target, "hasMagic", HasMagic);
  SetMethod(isolate, target, "globSync", GlobSync);
  SetMethod(isolate, target, "globStart", GlobStart);

  Local<FunctionTemplate> glob_request = NewFunctionTemplate(isolate, nullptr);
  glob_request->InstanceTemplate()->SetInternalFieldCount(
      AsyncWrap::kInternalFieldCount);
  glob_request->Inherit(AsyncWrap::GetConstructorTemplate(isolate_data));
  SetProtoMethod(isolate, glob_request, "next", GlobRequest::Next);
  SetProtoMethod(isolate, glob_request, "all", GlobRequest::All);
  SetProtoMethod(isolate, glob_request, "cancel", GlobRequest::Cancel);
  isolate_data->set_glob_request_template(glob_request);
}

void BindingData::CreatePerContextProperties(Local<Object> target,
                                             Local<Value> unused,
                                             Local<Context> context,
                                             void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  realm->AddBindingData<BindingData>(target);
  // The flag bits JavaScript may pass back in; the host-only bits stay
  // private to this file.
  NODE_DEFINE_CONSTANT(target, kFlagWindows);
  NODE_DEFINE_CONSTANT(target, kFlagDot);
  NODE_DEFINE_CONSTANT(target, kFlagFollowSymlinks);
  NODE_DEFINE_CONSTANT(target, kFlagWithFileTypes);
  NODE_DEFINE_CONSTANT(target, kFlagMatchBase);
}

void BindingData::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(MatchesGlob);
  registry->Register(HasMagic);
  registry->Register(GlobSync);
  registry->Register(GlobStart);
  registry->Register(GlobRequest::Next);
  registry->Register(GlobRequest::All);
  registry->Register(GlobRequest::Cancel);
}

}  // namespace node::glob

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    glob, node::glob::BindingData::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(
    glob, node::glob::BindingData::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    glob, node::glob::BindingData::RegisterExternalReferences)
