// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/codegen/code-desc.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/spaces.h"
#include "src/libsampler/sampler.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_code_pages {

// We have three levels of support which have different behaviors to test.
// 1 - Have code range. ARM64 and x64
// 2 - Have code pages. ARM32 only
// 3 - Nothing - This feature does not work on other platforms.
#if defined(V8_TARGET_ARCH_ARM)
static const bool kHaveCodePages = true;
#else
static const bool kHaveCodePages = false;
#endif  // defined(V8_TARGET_ARCH_ARM)

static const char* foo_source = R"(
  function foo%d(a, b) {
    let x = a * b;
    let y = x ^ b;
    let z = y / a;
    return x + y - z;
  };
  %%PrepareFunctionForOptimization(foo%d);
  foo%d(1, 2);
  foo%d(1, 2);
  %%OptimizeFunctionOnNextCall(foo%d);
  foo%d(1, 2);
)";

std::string getFooCode(int n) {
  constexpr size_t kMaxSize = 512;
  char foo_replaced[kMaxSize];
  CHECK_LE(n, 999999);
  snprintf(foo_replaced, kMaxSize, foo_source, n, n, n, n, n, n);

  return std::string(foo_replaced);
}

namespace {

bool PagesHasExactPage(std::vector<MemoryRange>* pages, Address search_page) {
  void* addr = reinterpret_cast<void*>(search_page);
  auto it =
      std::find_if(pages->begin(), pages->end(),
                   [addr](const MemoryRange& r) { return r.start == addr; });
  return it != pages->end();
}

bool PagesHasExactPage(std::vector<MemoryRange>* pages, Address search_page,
                       size_t size) {
  void* addr = reinterpret_cast<void*>(search_page);
  auto it = std::find_if(pages->begin(), pages->end(),
                         [addr, size](const MemoryRange& r) {
                           return r.start == addr && r.length_in_bytes == size;
                         });
  return it != pages->end();
}

bool PagesContainsAddress(std::vector<MemoryRange>* pages,
                          Address search_address) {
  byte* addr = reinterpret_cast<byte*>(search_address);
  auto it =
      std::find_if(pages->begin(), pages->end(), [addr](const MemoryRange& r) {
        const byte* page_start = reinterpret_cast<const byte*>(r.start);
        const byte* page_end = page_start + r.length_in_bytes;
        return addr >= page_start && addr < page_end;
      });
  return it != pages->end();
}

}  // namespace

TEST(CodeRangeCorrectContents) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  if (!i_isolate->RequiresCodeRange()) return;

  std::vector<MemoryRange>* pages = i_isolate->GetCodePages();

  const base::AddressRegion& code_range =
      i_isolate->heap()->memory_allocator()->code_range();
  CHECK(!code_range.is_empty());
  // We should only have the code range and the embedded code range.
  CHECK_EQ(2, pages->size());
  CHECK(PagesHasExactPage(pages, code_range.begin(), code_range.size()));
  CHECK(PagesHasExactPage(pages,
                          reinterpret_cast<Address>(i_isolate->embedded_blob()),
                          i_isolate->embedded_blob_size()));
}

TEST(CodePagesCorrectContents) {
  if (!kHaveCodePages) return;

  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  std::vector<MemoryRange>* pages = i_isolate->GetCodePages();
  // There might be other pages already.
  CHECK_GE(pages->size(), 1);

  const base::AddressRegion& code_range =
      i_isolate->heap()->memory_allocator()->code_range();
  CHECK(code_range.is_empty());

  // We should have the embedded code range even when there is no regular code
  // range.
  CHECK(PagesHasExactPage(pages,
                          reinterpret_cast<Address>(i_isolate->embedded_blob()),
                          i_isolate->embedded_blob_size()));
}

TEST(OptimizedCodeWithCodeRange) {
  FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  if (!i_isolate->RequiresCodeRange()) return;

  HandleScope scope(i_isolate);

  std::string foo_str = getFooCode(1);
  CompileRun(foo_str.c_str());
  v8::Local<v8::Function> local_foo = v8::Local<v8::Function>::Cast(
      env.local()->Global()->Get(env.local(), v8_str("foo1")).ToLocalChecked());
  Handle<JSFunction> foo =
      Handle<JSFunction>::cast(v8::Utils::OpenHandle(*local_foo));

  AbstractCode abstract_code = foo->abstract_code();
  // We don't produce optimized code when run with --no-opt.
  if (!abstract_code.IsCode() && FLAG_opt == false) return;
  CHECK(abstract_code.IsCode());
  Code foo_code = abstract_code.GetCode();

  CHECK(i_isolate->heap()->InSpace(foo_code, CODE_SPACE));

  std::vector<MemoryRange>* pages = i_isolate->GetCodePages();
  CHECK(PagesContainsAddress(pages, foo_code.address()));
}

TEST(OptimizedCodeWithCodePages) {
  if (!kHaveCodePages) return;
  // We don't want incremental marking to start which could cause the code to
  // not be collected on the CollectGarbage() call.
  ManualGCScope manual_gc_scope;
  FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  const void* created_page = nullptr;
  int num_foos_created = 0;

  {
    HandleScope scope(i_isolate);

    size_t num_code_pages = 0;
    size_t initial_num_code_pages = 0;

    // Keep generating new code until a new code page is added to the list.
    for (int n = 0; n < 999999; n++) {
      // Compile and optimize the code and get a reference to it.
      std::string foo_str = getFooCode(n);
      char foo_name[10];
      snprintf(foo_name, sizeof(foo_name), "foo%d", n);
      CompileRun(foo_str.c_str());
      v8::Local<v8::Function> local_foo =
          v8::Local<v8::Function>::Cast(env.local()
                                            ->Global()
                                            ->Get(env.local(), v8_str(foo_name))
                                            .ToLocalChecked());
      Handle<JSFunction> foo =
          Handle<JSFunction>::cast(v8::Utils::OpenHandle(*local_foo));

      AbstractCode abstract_code = foo->abstract_code();
      // We don't produce optimized code when run with --no-opt.
      if (!abstract_code.IsCode() && FLAG_opt == false) return;
      CHECK(abstract_code.IsCode());
      Code foo_code = abstract_code.GetCode();

      CHECK(i_isolate->heap()->InSpace(foo_code, CODE_SPACE));

      // Check that the generated code ended up in one of the code pages
      // returned by GetCodePages().
      byte* foo_code_ptr = reinterpret_cast<byte*>(foo_code.address());
      std::vector<MemoryRange>* pages = i_isolate->GetCodePages();

      // Wait until after we have created the first function to take the initial
      // number of pages so that this test isn't brittle to irrelevant
      // implementation details.
      if (n == 0) {
        initial_num_code_pages = pages->size();
      }
      num_code_pages = pages->size();

      // Check that the code object was allocation on any of the pages returned
      // by GetCodePages().
      auto it = std::find_if(
          pages->begin(), pages->end(), [foo_code_ptr](const MemoryRange& r) {
            const byte* page_start = reinterpret_cast<const byte*>(r.start);
            const byte* page_end = page_start + r.length_in_bytes;
            return foo_code_ptr >= page_start && foo_code_ptr < page_end;
          });
      CHECK_NE(it, pages->end());

      // Store the page that was created just for our functions - we expect it
      // to be removed later.
      if (num_code_pages > initial_num_code_pages) {
        created_page = it->start;
        num_foos_created = n + 1;
        break;
      }
    }
    CHECK_NOT_NULL(created_page);
  }

  // Now delete all our foos and force a GC and check that the page is removed
  // from the list.
  {
    HandleScope scope(i_isolate);
    for (int n = 0; n < num_foos_created; n++) {
      char foo_name[10];
      snprintf(foo_name, sizeof(foo_name), "foo%d", n);
      env.local()
          ->Global()
          ->Set(env.local(), v8_str(foo_name), Undefined(isolate))
          .Check();
    }
  }

  CcTest::CollectGarbage(CODE_SPACE);

  std::vector<MemoryRange>* pages = i_isolate->GetCodePages();
  auto it = std::find_if(
      pages->begin(), pages->end(),
      [created_page](const MemoryRange& r) { return r.start == created_page; });
  CHECK_EQ(it, pages->end());
}

TEST(LargeCodeObject) {
  // We don't want incremental marking to start which could cause the code to
  // not be collected on the CollectGarbage() call.
  ManualGCScope manual_gc_scope;

  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  if (!i_isolate->RequiresCodeRange() && !kHaveCodePages) return;

  // Create a big function that ends up in CODE_LO_SPACE.
  const int instruction_size = Page::kPageSize + 1;
  STATIC_ASSERT(instruction_size > kMaxRegularHeapObjectSize);
  std::unique_ptr<byte[]> instructions(new byte[instruction_size]);

  CodeDesc desc;
  desc.buffer = instructions.get();
  desc.buffer_size = instruction_size;
  desc.instr_size = instruction_size;
  desc.reloc_size = 0;
  desc.constant_pool_size = 0;
  desc.unwinding_info = nullptr;
  desc.unwinding_info_size = 0;
  desc.origin = nullptr;

  Address stale_code_address;

  {
    HandleScope scope(i_isolate);
    Handle<Code> foo_code =
        Factory::CodeBuilder(i_isolate, desc, Code::WASM_FUNCTION).Build();

    CHECK(i_isolate->heap()->InSpace(*foo_code, CODE_LO_SPACE));

    std::vector<MemoryRange>* pages = i_isolate->GetCodePages();

    if (i_isolate->RequiresCodeRange()) {
      CHECK(PagesContainsAddress(pages, foo_code->address()));
    } else {
      CHECK(PagesHasExactPage(pages, foo_code->address()));
    }

    stale_code_address = foo_code->address();
  }

  // Delete the large code object.
  CcTest::CollectGarbage(CODE_LO_SPACE);
  CHECK(!i_isolate->heap()->InSpaceSlow(stale_code_address, CODE_LO_SPACE));

  // Check that it was removed from CodePages.
  std::vector<MemoryRange>* pages = i_isolate->GetCodePages();
  CHECK(!PagesHasExactPage(pages, stale_code_address));
}

static constexpr size_t kBufSize = v8::Isolate::kMinCodePagesBufferSize;

class SignalSender : public sampler::Sampler {
 public:
  explicit SignalSender(v8::Isolate* isolate) : sampler::Sampler(isolate) {}

  // Called during the signal/thread suspension.
  void SampleStack(const v8::RegisterState& regs) override {
    MemoryRange* code_pages_copy = code_pages_copy_.load();
    CHECK_NOT_NULL(code_pages_copy);
    size_t num_pages = isolate_->CopyCodePages(kBufSize, code_pages_copy);
    CHECK_LE(num_pages, kBufSize);
    sample_semaphore_.Signal();
  }

  // Called on the sampling thread to trigger a sample. Blocks until the sample
  // is finished.
  void SampleIntoVector(MemoryRange output_buffer[]) {
    code_pages_copy_.store(output_buffer);
    DoSample();
    sample_semaphore_.Wait();
    code_pages_copy_.store(nullptr);
  }

 private:
  base::Semaphore sample_semaphore_{0};
  std::atomic<MemoryRange*> code_pages_copy_{nullptr};
};

class SamplingThread : public base::Thread {
 public:
  explicit SamplingThread(SignalSender* signal_sender)
      : base::Thread(base::Thread::Options("SamplingThread")),
        signal_sender_(signal_sender) {}

  // Blocks until a sample is taken.
  void TriggerSample() { signal_sender_->SampleIntoVector(code_pages_copy_); }

  void Run() override {
    while (running_.load()) {
      TriggerSample();
    }
  }

  // Called from the main thread. Blocks until a sample is taken. Not
  // thread-safe so do not call while this thread is running.
  static std::vector<MemoryRange> DoSynchronousSample(v8::Isolate* isolate) {
    MemoryRange code_pages_copy[kBufSize];
    size_t num_pages = isolate->CopyCodePages(kBufSize, code_pages_copy);
    DCHECK_LE(num_pages, kBufSize);
    return std::vector<MemoryRange>{code_pages_copy,
                                    &code_pages_copy[num_pages]};
  }

  void Stop() { running_.store(false); }

 private:
  std::atomic_bool running_{true};
  SignalSender* signal_sender_;
  MemoryRange code_pages_copy_[kBufSize];
};

TEST(LargeCodeObjectWithSignalHandler) {
  // We don't want incremental marking to start which could cause the code to
  // not be collected on the CollectGarbage() call.
  ManualGCScope manual_gc_scope;

  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  if (!i_isolate->RequiresCodeRange() && !kHaveCodePages) return;

  // Create a big function that ends up in CODE_LO_SPACE.
  const int instruction_size = Page::kPageSize + 1;
  STATIC_ASSERT(instruction_size > kMaxRegularHeapObjectSize);
  std::unique_ptr<byte[]> instructions(new byte[instruction_size]);

  CodeDesc desc;
  desc.buffer = instructions.get();
  desc.buffer_size = instruction_size;
  desc.instr_size = instruction_size;
  desc.reloc_size = 0;
  desc.constant_pool_size = 0;
  desc.unwinding_info = nullptr;
  desc.unwinding_info_size = 0;
  desc.origin = nullptr;

  Address stale_code_address;

  SignalSender signal_sender(isolate);
  signal_sender.Start();
  // Take an initial sample.
  std::vector<MemoryRange> initial_pages =
      SamplingThread::DoSynchronousSample(isolate);
  SamplingThread sampling_thread(&signal_sender);

  sampling_thread.StartSynchronously();

  {
    HandleScope scope(i_isolate);
    Handle<Code> foo_code =
        Factory::CodeBuilder(i_isolate, desc, Code::WASM_FUNCTION).Build();

    CHECK(i_isolate->heap()->InSpace(*foo_code, CODE_LO_SPACE));

    // Do a synchronous sample to ensure that we capture the state with the
    // extra code page.
    sampling_thread.Stop();
    sampling_thread.Join();

    // Check that the page was added.
    std::vector<MemoryRange> pages =
        SamplingThread::DoSynchronousSample(isolate);
    if (i_isolate->RequiresCodeRange()) {
      CHECK(PagesContainsAddress(&pages, foo_code->address()));
    } else {
      CHECK(PagesHasExactPage(&pages, foo_code->address()));
    }

    stale_code_address = foo_code->address();
  }

  // Start async sampling again to detect threading issues.
  sampling_thread.StartSynchronously();

  // Delete the large code object.
  CcTest::CollectGarbage(CODE_LO_SPACE);
  CHECK(!i_isolate->heap()->InSpaceSlow(stale_code_address, CODE_LO_SPACE));

  sampling_thread.Stop();
  sampling_thread.Join();

  std::vector<MemoryRange> pages = SamplingThread::DoSynchronousSample(isolate);
  CHECK(!PagesHasExactPage(&pages, stale_code_address));

  signal_sender.Stop();
}

TEST(Sorted) {
  // We don't want incremental marking to start which could cause the code to
  // not be collected on the CollectGarbage() call.
  ManualGCScope manual_gc_scope;

  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  if (!i_isolate->RequiresCodeRange() && !kHaveCodePages) return;

  // Create a big function that ends up in CODE_LO_SPACE.
  const int instruction_size = Page::kPageSize + 1;
  STATIC_ASSERT(instruction_size > kMaxRegularHeapObjectSize);
  std::unique_ptr<byte[]> instructions(new byte[instruction_size]);

  CodeDesc desc;
  desc.buffer = instructions.get();
  desc.buffer_size = instruction_size;
  desc.instr_size = instruction_size;
  desc.reloc_size = 0;
  desc.constant_pool_size = 0;
  desc.unwinding_info = nullptr;
  desc.unwinding_info_size = 0;
  desc.origin = nullptr;

  // Take an initial sample.
  std::vector<MemoryRange> initial_pages =
      SamplingThread::DoSynchronousSample(isolate);
  size_t initial_num_pages = initial_pages.size();

  auto compare = [](const MemoryRange& a, const MemoryRange& b) {
    return a.start < b.start;
  };
  {
    HandleScope outer_scope(i_isolate);
    Handle<Code> code1, code3;
    Address code2_address;

    code1 = Factory::CodeBuilder(i_isolate, desc, Code::WASM_FUNCTION).Build();
    CHECK(i_isolate->heap()->InSpace(*code1, CODE_LO_SPACE));

    {
      HandleScope scope(i_isolate);

      // Create three large code objects, we'll delete the middle one and check
      // everything is still sorted.
      Handle<Code> code2 =
          Factory::CodeBuilder(i_isolate, desc, Code::WASM_FUNCTION).Build();
      CHECK(i_isolate->heap()->InSpace(*code2, CODE_LO_SPACE));
      code3 =
          Factory::CodeBuilder(i_isolate, desc, Code::WASM_FUNCTION).Build();
      CHECK(i_isolate->heap()->InSpace(*code3, CODE_LO_SPACE));

      code2_address = code2->address();
      CHECK(i_isolate->heap()->InSpaceSlow(code1->address(), CODE_LO_SPACE));
      CHECK(i_isolate->heap()->InSpaceSlow(code2->address(), CODE_LO_SPACE));
      CHECK(i_isolate->heap()->InSpaceSlow(code3->address(), CODE_LO_SPACE));

      // Check that the pages were added.
      std::vector<MemoryRange> pages =
          SamplingThread::DoSynchronousSample(isolate);
      if (i_isolate->RequiresCodeRange()) {
        CHECK_EQ(pages.size(), initial_num_pages);
      } else {
        CHECK_EQ(pages.size(), initial_num_pages + 3);
      }

      CHECK(std::is_sorted(pages.begin(), pages.end(), compare));

      code3 = scope.CloseAndEscape(code3);
    }
    CHECK(i_isolate->heap()->InSpaceSlow(code1->address(), CODE_LO_SPACE));
    CHECK(i_isolate->heap()->InSpaceSlow(code2_address, CODE_LO_SPACE));
    CHECK(i_isolate->heap()->InSpaceSlow(code3->address(), CODE_LO_SPACE));
    // Delete code2.
    CcTest::CollectGarbage(CODE_LO_SPACE);
    CHECK(i_isolate->heap()->InSpaceSlow(code1->address(), CODE_LO_SPACE));
    CHECK(!i_isolate->heap()->InSpaceSlow(code2_address, CODE_LO_SPACE));
    CHECK(i_isolate->heap()->InSpaceSlow(code3->address(), CODE_LO_SPACE));

    std::vector<MemoryRange> pages =
        SamplingThread::DoSynchronousSample(isolate);
    if (i_isolate->RequiresCodeRange()) {
      CHECK_EQ(pages.size(), initial_num_pages);
    } else {
      CHECK_EQ(pages.size(), initial_num_pages + 2);
    }
    CHECK(std::is_sorted(pages.begin(), pages.end(), compare));
  }
}

}  // namespace test_code_pages
}  // namespace internal
}  // namespace v8
