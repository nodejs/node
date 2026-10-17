#ifndef SRC_GLOB_NODE_GLOB_H_
#define SRC_GLOB_NODE_GLOB_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>
#include <string>
#include <string_view>

#include "async_wrap.h"
#include "glob/glob_program.h"
#include "glob/glob_walker.h"
#include "lru_cache-inl.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_snapshotable.h"
#include "threadpoolwork-inl.h"

namespace node::glob {

// Consults a JavaScript `exclude` callback.
class JsExcludeFilter final : public ExcludeFilter {
 public:
  JsExcludeFilter(Environment* env, v8::Local<v8::Function> callback);

  bool ExcludesPath(std::string_view path) override;
  bool ExcludesEntry(std::string_view name,
                     std::string_view parent_path,
                     int type) override;
  bool failed() const override { return failed_; }

 private:
  // Nothing when the callback threw; the exception stays pending.
  v8::Maybe<bool> Call(bool entry,
                       std::string_view first,
                       std::string_view second,
                       int type);

  Environment* env_;
  v8::Global<v8::Function> callback_;
  bool failed_ = false;
};

// The reads a JavaScript reader serves, see internal/fs/glob.
enum ReadOp : int {
  kReadLstat = 0,
  kReadStat = 1,
  kReadRealpath = 2,
  kReadScandir = 3,
};

// Reads the filesystem through a JavaScript function: the fs hooks of a
// mounted virtual file system.
class JsFsReader final : public FsReader {
 public:
  JsFsReader(Environment* env, v8::Local<v8::Function> read);

  StatInfo Lstat(const std::string& path) override;
  bool IsDirectory(const std::string& path) override;
  std::string Realpath(const std::string& path) override;
  std::vector<DirEntry> Scandir(const std::string& path) override;
  bool failed() const override { return failed_; }

 private:
  // Nothing when the function threw; the exception stays pending.
  v8::MaybeLocal<v8::Value> Call(ReadOp op, const std::string& path);

  Environment* env_;
  v8::Global<v8::Function> read_;
  bool failed_ = false;
};

class BindingData : public SnapshotableObject {
 public:
  BindingData(Realm* realm,
              v8::Local<v8::Object> object,
              InternalFieldInfoBase* info = nullptr);

  using InternalFieldInfo = InternalFieldInfoBase;

  SERIALIZABLE_OBJECT_METHODS()
  SET_BINDING_ID(glob_binding_data)

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(BindingData)
  SET_MEMORY_INFO_NAME(BindingData)

  static void MatchesGlob(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HasMagic(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GlobSync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GlobStart(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void CreatePerIsolateProperties(IsolateData* isolate_data,
                                         v8::Local<v8::ObjectTemplate> target);
  static void CreatePerContextProperties(v8::Local<v8::Object> target,
                                         v8::Local<v8::Value> unused,
                                         v8::Local<v8::Context> context,
                                         void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  bool ReadWalkArguments(const v8::FunctionCallbackInfo<v8::Value>& args,
                         WalkOptions* options,
                         std::vector<CompiledPatternPtr>* includes,
                         std::vector<CompiledPatternPtr>* excludes,
                         std::unique_ptr<JsExcludeFilter>* filter,
                         std::unique_ptr<JsFsReader>* reader);

  // Compiles (or fetches) `pattern`; returns nullptr on a compile error,
  // with the reason written to *error.
  CompiledPatternPtr Lookup(PatternView pattern,
                            const CompileFlags& flags,
                            CompileError* error);

 private:
  struct CacheEntry {
    CompiledPattern compiled;
    // Set when the pattern does not compile.
    CompileError error = CompileError::kNone;
  };

  // A compiled pattern is large, so we limit the size
  // of the cache.
  static constexpr size_t kMaxCacheEntries = 256;

  // Compiled patterns, keyed by the pattern text + flag
  LRUCache<PatternString, std::shared_ptr<CacheEntry>> cache_{kMaxCacheEntries};
  PatternString lookup_key_;
};

class GlobRequest : public AsyncWrap, public ThreadPoolWork {
 public:
  GlobRequest(Environment* env,
              v8::Local<v8::Object> object,
              const WalkOptions& options,
              const std::vector<CompiledPatternPtr>& includes,
              const std::vector<CompiledPatternPtr>& excludes,
              std::unique_ptr<JsExcludeFilter> filter,
              std::unique_ptr<JsFsReader> reader);

  // How many results an iterator pull returns at a time.
  // See the explanation in this header's .cc file for
  // more information.
  static constexpr size_t kBatchSize = 256;

  static void Next(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void All(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Cancel(const v8::FunctionCallbackInfo<v8::Value>& args);

  void DoThreadPoolWork() override;
  void AfterThreadPoolWork(int status) override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(GlobRequest)
  SET_SELF_SIZE(GlobRequest)

 private:
  static void Pull(const v8::FunctionCallbackInfo<v8::Value>& args, bool drain);
  // How many results the pull in flight still wants.
  size_t MaxResults() const;
  // A slice of a walk that reads through JavaScript, run on the main
  // thread from a later loop turn.
  void RunMainThreadSlice();
  // Settles the pull in flight: with the batch, or with the exception a
  // callback threw.
  void Resolve();
  void Reject(const errors::TryCatchScope& try_catch);
  v8::MaybeLocal<v8::Value> Settle();
  // The main-thread half of a slice: publishes the permission denials the
  // walk met and puts the questions of a directory waiting on the exclude
  // callback to it, in the order the walk would have.
  void PublishDenials();
  void AnswerQuestions();

  // Declared before the walk, which points at them.
  std::unique_ptr<JsExcludeFilter> filter_;
  std::unique_ptr<JsFsReader> reader_;
  Walk walk_;
  const bool with_file_types_;
  // Set by all(): the pull consumes the rest of the walk in one batch.
  bool drain_ = false;
  bool done_ = false;
  bool cancelled_ = false;
  bool in_flight_ = false;
  std::vector<WalkEntry> batch_;
  v8::Global<v8::Promise::Resolver> resolver_;
};

}  // namespace node::glob

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_GLOB_NODE_GLOB_H_
