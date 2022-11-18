// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-context.h"
#include "include/v8-function.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-platform.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

class DeserializeTest : public TestWithPlatform {
 public:
  class IsolateAndContextScope {
   public:
    explicit IsolateAndContextScope(DeserializeTest* test)
        : test_(test),
          isolate_wrapper_(kNoCounters),
          isolate_scope_(isolate_wrapper_.isolate()),
          handle_scope_(isolate_wrapper_.isolate()),
          context_(Context::New(isolate_wrapper_.isolate())),
          context_scope_(context_) {
      CHECK_NULL(test->isolate_);
      CHECK(test->context_.IsEmpty());
      test->isolate_ = isolate_wrapper_.isolate();
      test->context_ = context_;
    }
    ~IsolateAndContextScope() {
      test_->isolate_ = nullptr;
      test_->context_ = {};
    }

   private:
    DeserializeTest* test_;
    v8::IsolateWrapper isolate_wrapper_;
    v8::Isolate::Scope isolate_scope_;
    v8::HandleScope handle_scope_;
    v8::Local<v8::Context> context_;
    v8::Context::Scope context_scope_;
  };

  Local<String> NewString(const char* val) {
    return String::NewFromUtf8(isolate(), val).ToLocalChecked();
  }

  Local<Value> RunGlobalFunc(const char* name) {
    Local<Value> func_val =
        context()->Global()->Get(context(), NewString(name)).ToLocalChecked();
    CHECK(func_val->IsFunction());
    Local<Function> func = Local<Function>::Cast(func_val);
    return func->Call(context(), Undefined(isolate()), 0, nullptr)
        .ToLocalChecked();
  }

  Isolate* isolate() { return isolate_; }
  v8::Local<v8::Context> context() { return context_.ToLocalChecked(); }

 private:
  Isolate* isolate_ = nullptr;
  v8::MaybeLocal<v8::Context> context_;
};

// Check that deserialization works.
TEST_F(DeserializeTest, Deserialize) {
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    Local<Script> script =
        Script::Compile(context(), source_code).ToLocalChecked();

    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), Integer::New(isolate(), 42));

    cached_data.reset(
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  }

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    ScriptCompiler::Source source(source_code, cached_data.release());
    Local<Script> script =
        ScriptCompiler::Compile(context(), &source,
                                ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(!source.GetCachedData()->rejected);
    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), v8::Integer::New(isolate(), 42));
  }
}

// Check that deserialization with a different script rejects the cache but
// still works via standard compilation.
TEST_F(DeserializeTest, DeserializeRejectsDifferentSource) {
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    Local<Script> script =
        Script::Compile(context(), source_code).ToLocalChecked();

    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), Integer::New(isolate(), 42));

    cached_data.reset(
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  }

  {
    IsolateAndContextScope scope(this);

    // The source hash is based on the source length, so have to make sure that
    // this is different here.
    Local<String> source_code = NewString("function bar() { return 142; }");
    ScriptCompiler::Source source(source_code, cached_data.release());
    Local<Script> script =
        ScriptCompiler::Compile(context(), &source,
                                ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(source.GetCachedData()->rejected);
    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("bar"), v8::Integer::New(isolate(), 142));
  }
}

class DeserializeThread : public base::Thread {
 public:
  explicit DeserializeThread(ScriptCompiler::ConsumeCodeCacheTask* task)
      : Thread(base::Thread::Options("DeserializeThread")), task_(task) {}

  void Run() override { task_->Run(); }

  std::unique_ptr<ScriptCompiler::ConsumeCodeCacheTask> TakeTask() {
    return std::move(task_);
  }

 private:
  std::unique_ptr<ScriptCompiler::ConsumeCodeCacheTask> task_;
};

// Check that off-thread deserialization works.
TEST_F(DeserializeTest, OffThreadDeserialize) {
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    Local<Script> script =
        Script::Compile(context(), source_code).ToLocalChecked();

    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), Integer::New(isolate(), 42));

    cached_data.reset(
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  }

  {
    IsolateAndContextScope scope(this);

    DeserializeThread deserialize_thread(
        ScriptCompiler::StartConsumingCodeCache(
            isolate(), std::make_unique<ScriptCompiler::CachedData>(
                           cached_data->data, cached_data->length,
                           ScriptCompiler::CachedData::BufferNotOwned)));
    CHECK(deserialize_thread.Start());
    deserialize_thread.Join();

    Local<String> source_code = NewString("function foo() { return 42; }");
    ScriptCompiler::Source source(source_code, cached_data.release(),
                                  deserialize_thread.TakeTask().release());
    Local<Script> script =
        ScriptCompiler::Compile(context(), &source,
                                ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(!source.GetCachedData()->rejected);
    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), v8::Integer::New(isolate(), 42));
  }
}

// Check that off-thread deserialization works.
TEST_F(DeserializeTest, OffThreadDeserializeRejectsDifferentSource) {
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    Local<Script> script =
        Script::Compile(context(), source_code).ToLocalChecked();

    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), Integer::New(isolate(), 42));

    cached_data.reset(
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  }

  {
    IsolateAndContextScope scope(this);

    DeserializeThread deserialize_thread(
        ScriptCompiler::StartConsumingCodeCache(
            isolate(), std::make_unique<ScriptCompiler::CachedData>(
                           cached_data->data, cached_data->length,
                           ScriptCompiler::CachedData::BufferNotOwned)));
    CHECK(deserialize_thread.Start());
    deserialize_thread.Join();

    Local<String> source_code = NewString("function bar() { return 142; }");
    ScriptCompiler::Source source(source_code, cached_data.release(),
                                  deserialize_thread.TakeTask().release());
    Local<Script> script =
        ScriptCompiler::Compile(context(), &source,
                                ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(source.GetCachedData()->rejected);
    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("bar"), v8::Integer::New(isolate(), 142));
  }
}

class MergeDeserializedCodeTest : public DeserializeTest {
 protected:
  // The source code used in these tests.
  static constexpr char kSourceCode[] = R"(
    // Looks like an IIFE but isn't, to get eagerly parsed:
    var eager = (function () {
      // Actual IIFE, also eagerly parsed:
      return (function iife() {
        return 42;
      })();
    });
    // Lazily parsed:
    var lazy = function () { return eager(); };
  )";

  // Objects from the Script's object graph whose lifetimes and connectedness
  // are useful to track.
  enum ScriptObject {
    kScript,
    kToplevelSfi,
    kToplevelFunctionData,
    kToplevelFeedbackMetadata,
    kEagerSfi,
    kEagerFunctionData,
    kEagerFeedbackMetadata,
    kIifeSfi,
    kIifeFunctionData,
    kIifeFeedbackMetadata,
    kLazySfi,
    kScriptObjectsCount
  };
  enum ScriptObjectFlag {
    kNone,

    kScriptFlag = 1 << kScript,
    kToplevelSfiFlag = 1 << kToplevelSfi,
    kToplevelFunctionDataFlag = 1 << kToplevelFunctionData,
    kToplevelFeedbackMetadataFlag = 1 << kToplevelFeedbackMetadata,
    kEagerSfiFlag = 1 << kEagerSfi,
    kEagerFunctionDataFlag = 1 << kEagerFunctionData,
    kEagerFeedbackMetadataFlag = 1 << kEagerFeedbackMetadata,
    kIifeSfiFlag = 1 << kIifeSfi,
    kIifeFunctionDataFlag = 1 << kIifeFunctionData,
    kIifeFeedbackMetadataFlag = 1 << kIifeFeedbackMetadata,
    kLazySfiFlag = 1 << kLazySfi,

    kAllScriptObjects = (1 << kScriptObjectsCount) - 1,
    kAllCompiledSfis = kToplevelSfiFlag | kEagerSfiFlag | kIifeSfiFlag,
    kAllSfis = kAllCompiledSfis | kLazySfiFlag,
    kEagerAndLazy = kLazySfiFlag | kEagerSfiFlag,
    kToplevelEagerAndLazy = kToplevelSfiFlag | kEagerAndLazy,
    kToplevelAndEager = kToplevelSfiFlag | kEagerSfiFlag,
  };

  template <typename T>
  static i::SharedFunctionInfo GetSharedFunctionInfo(
      Local<T> function_or_script) {
    i::Handle<i::JSFunction> i_function =
        i::Handle<i::JSFunction>::cast(Utils::OpenHandle(*function_or_script));
    return i_function->shared();
  }

  static i::MaybeObject WeakOrSmi(i::Object obj) {
    return obj.IsSmi()
               ? i::MaybeObject::FromSmi(i::Smi::cast(obj))
               : i::MaybeObject::MakeWeak(i::MaybeObject::FromObject(obj));
  }

  void ValidateStandaloneGraphAndPopulateArray(
      i::SharedFunctionInfo toplevel_sfi, i::WeakFixedArray array,
      bool lazy_should_be_compiled = false,
      bool eager_should_be_compiled = true) {
    i::DisallowGarbageCollection no_gc;
    CHECK(toplevel_sfi.is_compiled());
    array.Set(kToplevelSfi, WeakOrSmi(toplevel_sfi));
    array.Set(kToplevelFunctionData,
              WeakOrSmi(toplevel_sfi.function_data(kAcquireLoad)));
    array.Set(kToplevelFeedbackMetadata,
              WeakOrSmi(toplevel_sfi.feedback_metadata()));
    i::Script script = i::Script::cast(toplevel_sfi.script());
    array.Set(kScript, WeakOrSmi(script));
    i::WeakFixedArray sfis = script.shared_function_infos();
    CHECK_EQ(sfis.length(), 4);
    CHECK_EQ(sfis.Get(0), WeakOrSmi(toplevel_sfi));
    i::SharedFunctionInfo eager =
        i::SharedFunctionInfo::cast(sfis.Get(1).GetHeapObjectAssumeWeak());
    CHECK_EQ(eager.is_compiled(), eager_should_be_compiled);
    array.Set(kEagerSfi, WeakOrSmi(eager));
    if (eager_should_be_compiled) {
      array.Set(kEagerFunctionData,
                WeakOrSmi(eager.function_data(kAcquireLoad)));
      array.Set(kEagerFeedbackMetadata, WeakOrSmi(eager.feedback_metadata()));
      i::SharedFunctionInfo iife =
          i::SharedFunctionInfo::cast(sfis.Get(2).GetHeapObjectAssumeWeak());
      CHECK(iife.is_compiled());
      array.Set(kIifeSfi, WeakOrSmi(iife));
      array.Set(kIifeFunctionData, WeakOrSmi(iife.function_data(kAcquireLoad)));
      array.Set(kIifeFeedbackMetadata, WeakOrSmi(iife.feedback_metadata()));
    }
    i::SharedFunctionInfo lazy =
        i::SharedFunctionInfo::cast(sfis.Get(3).GetHeapObjectAssumeWeak());
    CHECK_EQ(lazy.is_compiled(), lazy_should_be_compiled);
    array.Set(kLazySfi, WeakOrSmi(lazy));
  }

  void AgeBytecodeAndGC(ScriptObjectFlag sfis_to_age,
                        i::Handle<i::WeakFixedArray> original_objects,
                        i::Isolate* i_isolate) {
    for (int index = 0; index < kScriptObjectsCount; ++index) {
      if ((sfis_to_age & (1 << index)) == (1 << index)) {
        i::BytecodeArray bytecode =
            i::SharedFunctionInfo::cast(
                original_objects->Get(index).GetHeapObjectAssumeWeak())
                .GetBytecodeArray(i_isolate);
        const int kAgingThreshold = 6;
        for (int j = 0; j < kAgingThreshold; ++j) {
          bytecode.MakeOlder();
        }
      }
    }
    i_isolate->heap()->CollectAllGarbage(i::Heap::kNoGCFlags,
                                         i::GarbageCollectionReason::kTesting);

    // A second round of GC is necessary in case incremental marking had already
    // started before the bytecode was aged.
    i_isolate->heap()->CollectAllGarbage(i::Heap::kNoGCFlags,
                                         i::GarbageCollectionReason::kTesting);
  }

  class MergeThread : public base::Thread {
   public:
    explicit MergeThread(ScriptCompiler::ConsumeCodeCacheTask* task)
        : Thread(base::Thread::Options("MergeThread")), task_(task) {}

    void Run() override { task_->MergeWithExistingScript(); }

   private:
    ScriptCompiler::ConsumeCodeCacheTask* task_;
  };

  void RetainObjects(ScriptObjectFlag to_retain,
                     i::WeakFixedArray original_objects,
                     i::FixedArray retained_original_objects,
                     i::Isolate* i_isolate) {
    for (int index = 0; index < kScriptObjectsCount; ++index) {
      if ((to_retain & (1 << index)) == (1 << index)) {
        i::MaybeObject maybe = original_objects.Get(index);
        if (i::HeapObject heap_object;
            maybe.GetHeapObjectIfWeak(&heap_object)) {
          retained_original_objects.set(index, heap_object);
          continue;
        }
      }
      retained_original_objects.set(
          index, i::ReadOnlyRoots(i_isolate).undefined_value());
    }
  }

  void TestOffThreadMerge(ScriptObjectFlag retained_before_background_merge,
                          ScriptObjectFlag aged_before_background_merge,
                          bool run_code_after_background_merge,
                          ScriptObjectFlag retained_after_background_merge,
                          ScriptObjectFlag aged_after_background_merge,
                          bool lazy_should_be_compiled = false,
                          bool eager_should_be_compiled = true) {
    i::v8_flags.merge_background_deserialized_script_with_compilation_cache =
        true;
    std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;
    IsolateAndContextScope scope(this);
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate());
    ScriptOrigin default_origin(isolate(), NewString(""));

    i::Handle<i::WeakFixedArray> original_objects =
        i_isolate->factory()->NewWeakFixedArray(kScriptObjectsCount);
    i::Handle<i::FixedArray> retained_original_objects =
        i_isolate->factory()->NewFixedArray(kScriptObjectsCount);
    i::Handle<i::WeakFixedArray> new_objects =
        i_isolate->factory()->NewWeakFixedArray(kScriptObjectsCount);
    Local<Script> original_script;

    // Compile the script for the first time, to both populate the Isolate
    // compilation cache and produce code cache data.
    {
      v8::EscapableHandleScope handle_scope(isolate());
      Local<Script> script =
          Script::Compile(context(), NewString(kSourceCode), &default_origin)
              .ToLocalChecked();

      ValidateStandaloneGraphAndPopulateArray(GetSharedFunctionInfo(script),
                                              *original_objects);

      RetainObjects(retained_before_background_merge, *original_objects,
                    *retained_original_objects, i_isolate);

      cached_data.reset(
          ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));

      if (run_code_after_background_merge) {
        // We must retain the v8::Script (a JSFunction) so we can run it later.
        original_script = handle_scope.Escape(script);
        // It doesn't make any sense to configure a test case which says it
        // doesn't want to retain the toplevel SFI but does want to run the
        // script later.
        CHECK(retained_before_background_merge & kToplevelSfiFlag);
      }
    }

    AgeBytecodeAndGC(aged_before_background_merge, original_objects, i_isolate);

    DeserializeThread deserialize_thread(
        ScriptCompiler::StartConsumingCodeCache(
            isolate(), std::make_unique<ScriptCompiler::CachedData>(
                           cached_data->data, cached_data->length,
                           ScriptCompiler::CachedData::BufferNotOwned)));
    CHECK(deserialize_thread.Start());
    deserialize_thread.Join();

    std::unique_ptr<ScriptCompiler::ConsumeCodeCacheTask> task =
        deserialize_thread.TakeTask();

    task->SourceTextAvailable(isolate(), NewString(kSourceCode),
                              default_origin);

    // If the top-level SFI was retained and not flushed, then no merge is
    // necessary because the results from the deserialization will be discarded.
    // If nothing at all was retained, then no merge is necessary because the
    // original Script is no longer in the compilation cache. Otherwise, a merge
    // is necessary.
    bool merge_expected =
        (retained_before_background_merge != kNone) &&
        (!(retained_before_background_merge & kToplevelSfiFlag) ||
         (aged_before_background_merge & kToplevelSfiFlag));
    CHECK_EQ(merge_expected, task->ShouldMergeWithExistingScript());

    if (merge_expected) {
      MergeThread merge_thread(task.get());
      CHECK(merge_thread.Start());
      merge_thread.Join();
    }

    if (run_code_after_background_merge) {
      CHECK(!original_script->Run(context()).IsEmpty());
      CHECK_EQ(RunGlobalFunc("lazy"), v8::Integer::New(isolate(), 42));
      ValidateStandaloneGraphAndPopulateArray(
          GetSharedFunctionInfo(original_script), *original_objects,
          true /*lazy_should_be_compiled*/);
    }

    RetainObjects(retained_after_background_merge, *original_objects,
                  *retained_original_objects, i_isolate);

    AgeBytecodeAndGC(aged_after_background_merge, original_objects, i_isolate);

    ScriptCompiler::Source source(NewString(kSourceCode), default_origin,
                                  cached_data.release(), task.release());
    Local<Script> script =
        ScriptCompiler::Compile(context(), &source,
                                ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(!source.GetCachedData()->rejected);
    ValidateStandaloneGraphAndPopulateArray(
        GetSharedFunctionInfo(script), *new_objects, lazy_should_be_compiled,
        eager_should_be_compiled);

    // At this point, the original_objects array might still have pointers to
    // some old discarded content, such as UncompiledData from flushed
    // functions. GC again to clear it all out.
    i_isolate->heap()->CollectAllGarbage(i::Heap::kNoGCFlags,
                                         i::GarbageCollectionReason::kTesting);

    // All tracked objects from the original Script should have been reused if
    // they're still alive.
    for (int index = 0; index < kScriptObjectsCount; ++index) {
      if (original_objects->Get(index).IsWeak() &&
          new_objects->Get(index).IsWeak()) {
        CHECK_EQ(original_objects->Get(index), new_objects->Get(index));
      }
    }

    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("lazy"), v8::Integer::New(isolate(), 42));
  }
};

TEST_F(MergeDeserializedCodeTest, NoMergeWhenAlreadyCompiled) {
  // Retain everything; age nothing.
  TestOffThreadMerge(kAllScriptObjects,  // retained_before_background_merge
                     kNone,              // aged_before_background_merge
                     false,              // run_code_after_background_merge
                     kAllScriptObjects,  // retained_after_background_merge
                     kNone);             // aged_after_background_merge
}

TEST_F(MergeDeserializedCodeTest, NoMergeWhenOriginalWasDiscarded) {
  // Retain nothing.
  TestOffThreadMerge(kNone,   // retained_before_background_merge
                     kNone,   // aged_before_background_merge
                     false,   // run_code_after_background_merge
                     kNone,   // retained_after_background_merge
                     kNone);  // aged_after_background_merge
}

TEST_F(MergeDeserializedCodeTest, NoMergeWhenOriginalWasDiscardedLate) {
  // The original top-level SFI is retained by the background merge task even
  // though other retainers are discarded.
  TestOffThreadMerge(kAllScriptObjects,  // retained_before_background_merge
                     kNone,              // aged_before_background_merge
                     false,              // run_code_after_background_merge
                     kNone,              // retained_after_background_merge
                     kNone);             // aged_after_background_merge
}

TEST_F(MergeDeserializedCodeTest, MergeIntoFlushedSFIs) {
  // Retain all SFIs but age them.
  TestOffThreadMerge(kAllSfis,          // retained_before_background_merge
                     kAllCompiledSfis,  // aged_before_background_merge
                     false,             // run_code_after_background_merge
                     kAllSfis,          // retained_after_background_merge
                     kNone);            // aged_after_background_merge
}

TEST_F(MergeDeserializedCodeTest, MergeBasic) {
  // Retain the eager and lazy functions; discard the top-level SFI.
  // This is a common scenario which requires a merge.
  TestOffThreadMerge(kEagerAndLazy,     // retained_before_background_merge
                     kToplevelSfiFlag,  // aged_before_background_merge
                     false,             // run_code_after_background_merge
                     kNone,             // retained_after_background_merge
                     kNone);            // aged_after_background_merge
}

TEST_F(MergeDeserializedCodeTest, MergeBasicWithFlushing) {
  // Retain the eager and lazy functions; discard the top-level SFI.
  // Also flush the eager function, which discards the IIFE.
  // This is a common scenario which requires a merge.
  TestOffThreadMerge(kEagerAndLazy,      // retained_before_background_merge
                     kToplevelAndEager,  // aged_before_background_merge
                     false,              // run_code_after_background_merge
                     kNone,              // retained_after_background_merge
                     kNone);             // aged_after_background_merge
}

TEST_F(MergeDeserializedCodeTest, MergeBasicWithLateFlushing) {
  // Flush the eager function after the background merge has taken place. In
  // this case, the data from the background thread points to the eager SFI but
  // not its bytecode, so the end result is that the eager SFI is not compiled
  // after completion on the main thread.
  TestOffThreadMerge(kEagerAndLazy,     // retained_before_background_merge
                     kToplevelSfiFlag,  // aged_before_background_merge
                     false,             // run_code_after_background_merge
                     kNone,             // retained_after_background_merge
                     kEagerSfiFlag,     // aged_after_background_merge
                     false,             // lazy_should_be_compiled
                     false);            // eager_should_be_compiled
}

TEST_F(MergeDeserializedCodeTest, RunScriptButNoReMergeNecessary) {
  // The original script is run after the background merge, causing the
  // top-level SFI and lazy SFI to become compiled. However, no SFIs are
  // created when running the script, so the main thread needn't redo the merge.
  TestOffThreadMerge(kToplevelEagerAndLazy,  // retained_before_background_merge
                     kToplevelSfiFlag,       // aged_before_background_merge
                     true,                   // run_code_after_background_merge
                     kAllScriptObjects,      // retained_after_background_merge
                     kNone,                  // aged_after_background_merge
                     true);                  // lazy_should_be_compiled
}

TEST_F(MergeDeserializedCodeTest, MainThreadReMerge) {
  // By flushing the eager SFI early, we cause the IIFE SFI to disappear
  // entirely. When the original script runs after the background merge, the
  // IIFE SFI is recreated. Thus, the main thread must redo the merge.
  TestOffThreadMerge(kToplevelEagerAndLazy,  // retained_before_background_merge
                     kToplevelAndEager,      // aged_before_background_merge
                     true,                   // run_code_after_background_merge
                     kAllScriptObjects,      // retained_after_background_merge
                     kToplevelSfiFlag,       // aged_after_background_merge
                     true);                  // lazy_should_be_compiled
}

TEST_F(MergeDeserializedCodeTest, Regress1360024) {
  // This test case triggers a re-merge on the main thread, similar to
  // MainThreadReMerge. However, it does not retain the lazy function's SFI at
  // any step, which causes the merge to use the SFI from the newly deserialized
  // script for that function. This exercises a bug in the original
  // implementation where the re-merging on the main thread would crash if the
  // merge algorithm had selected any uncompiled SFIs from the new script.
  TestOffThreadMerge(kToplevelAndEager,      // retained_before_background_merge
                     kToplevelAndEager,      // aged_before_background_merge
                     true,                   // run_code_after_background_merge
                     kToplevelAndEager,      // retained_after_background_merge
                     kToplevelSfiFlag,       // aged_after_background_merge
                     true);                  // lazy_should_be_compiled
}

TEST_F(MergeDeserializedCodeTest, MergeWithNoFollowUpWork) {
  i::v8_flags.merge_background_deserialized_script_with_compilation_cache =
      true;
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;
  IsolateAndContextScope scope(this);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate());
  ScriptOrigin default_origin(isolate(), NewString(""));

  constexpr char kSourceCode[] = "function f() {}";
  Local<Script> original_script;

  // Compile the script for the first time, to both populate the Isolate
  // compilation cache and produce code cache data.
  {
    v8::EscapableHandleScope handle_scope(isolate());
    Local<Script> script =
        Script::Compile(context(), NewString(kSourceCode), &default_origin)
            .ToLocalChecked();

    cached_data.reset(
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));

    // Retain the v8::Script (a JSFunction) so we can run it later.
    original_script = handle_scope.Escape(script);
  }

  // Age the top-level bytecode so that the Isolate compilation cache will
  // contain only the Script.
  i::BytecodeArray bytecode =
      GetSharedFunctionInfo(original_script).GetBytecodeArray(i_isolate);
  const int kAgingThreshold = 6;
  for (int j = 0; j < kAgingThreshold; ++j) {
    bytecode.MakeOlder();
  }
  i_isolate->heap()->CollectAllGarbage(i::Heap::kNoGCFlags,
                                       i::GarbageCollectionReason::kTesting);

  // A second round of GC is necessary in case incremental marking had already
  // started before the bytecode was aged.
  i_isolate->heap()->CollectAllGarbage(i::Heap::kNoGCFlags,
                                       i::GarbageCollectionReason::kTesting);

  DeserializeThread deserialize_thread(ScriptCompiler::StartConsumingCodeCache(
      isolate(), std::make_unique<ScriptCompiler::CachedData>(
                     cached_data->data, cached_data->length,
                     ScriptCompiler::CachedData::BufferNotOwned)));
  CHECK(deserialize_thread.Start());
  deserialize_thread.Join();

  std::unique_ptr<ScriptCompiler::ConsumeCodeCacheTask> task =
      deserialize_thread.TakeTask();

  // At this point, the cached script's top-level SFI is not compiled, so a
  // background merge is recommended.
  task->SourceTextAvailable(isolate(), NewString(kSourceCode), default_origin);

  CHECK(task->ShouldMergeWithExistingScript());

  // Run the original script, which will cause its top-level SFI to become
  // compiled again, and make the SFI for the nested function exist.
  CHECK(!original_script->Run(context()).IsEmpty());

  // The background merge does nothing and requests no follow-up work on the
  // main thread because the original script has the same SFIs at the same level
  // of compiledness.
  MergeThread merge_thread(task.get());
  CHECK(merge_thread.Start());
  merge_thread.Join();

  // Complete compilation on the main thread. Even though no follow-up work is
  // required, this step should reuse the original script.
  ScriptCompiler::Source source(NewString(kSourceCode), default_origin,
                                cached_data.release(), task.release());
  Local<Script> script =
      ScriptCompiler::Compile(context(), &source,
                              ScriptCompiler::kConsumeCodeCache)
          .ToLocalChecked();

  CHECK_EQ(GetSharedFunctionInfo(script),
           GetSharedFunctionInfo(original_script));
}

}  // namespace v8
