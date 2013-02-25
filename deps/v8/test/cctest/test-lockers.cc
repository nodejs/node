// Copyright 2007-2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <limits.h>

#include "v8.h"

#include "api.h"
#include "isolate.h"
#include "compilation-cache.h"
#include "execution.h"
#include "snapshot.h"
#include "platform.h"
#include "utils.h"
#include "cctest.h"
#include "parser.h"
#include "unicode-inl.h"

using ::v8::AccessorInfo;
using ::v8::Context;
using ::v8::Extension;
using ::v8::Function;
using ::v8::HandleScope;
using ::v8::Local;
using ::v8::Object;
using ::v8::ObjectTemplate;
using ::v8::Persistent;
using ::v8::Script;
using ::v8::String;
using ::v8::Value;
using ::v8::V8;


// Migrating an isolate
class KangarooThread : public v8::internal::Thread {
 public:
  KangarooThread(v8::Isolate* isolate,
                 v8::Handle<v8::Context> context, int value)
      : Thread("KangarooThread"),
        isolate_(isolate), context_(context), value_(value) {
  }

  void Run() {
    {
      v8::Locker locker(isolate_);
      v8::Isolate::Scope isolate_scope(isolate_);
      CHECK_EQ(isolate_, v8::internal::Isolate::Current());
      v8::HandleScope scope;
      v8::Context::Scope context_scope(context_);
      Local<Value> v = CompileRun("getValue()");
      CHECK(v->IsNumber());
      CHECK_EQ(30, static_cast<int>(v->NumberValue()));
    }
    {
      v8::Locker locker(isolate_);
      v8::Isolate::Scope isolate_scope(isolate_);
      v8::Context::Scope context_scope(context_);
      v8::HandleScope scope;
      Local<Value> v = CompileRun("getValue()");
      CHECK(v->IsNumber());
      CHECK_EQ(30, static_cast<int>(v->NumberValue()));
    }
    isolate_->Dispose();
  }

 private:
  v8::Isolate* isolate_;
  Persistent<v8::Context> context_;
  int value_;
};

// Migrates an isolate from one thread to another
TEST(KangarooIsolates) {
  v8::Isolate* isolate = v8::Isolate::New();
  Persistent<v8::Context> context;
  {
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope;
    context = v8::Context::New();
    v8::Context::Scope context_scope(context);
    CHECK_EQ(isolate, v8::internal::Isolate::Current());
    CompileRun("function getValue() { return 30; }");
  }
  KangarooThread thread1(isolate, context, 1);
  thread1.Start();
  thread1.Join();
}

static void CalcFibAndCheck() {
  Local<Value> v = CompileRun("function fib(n) {"
                              "  if (n <= 2) return 1;"
                              "  return fib(n-1) + fib(n-2);"
                              "}"
                              "fib(10)");
  CHECK(v->IsNumber());
  CHECK_EQ(55, static_cast<int>(v->NumberValue()));
}

class JoinableThread {
 public:
  explicit JoinableThread(const char* name)
    : name_(name),
      semaphore_(i::OS::CreateSemaphore(0)),
      thread_(this) {
  }

  virtual ~JoinableThread() {
    delete semaphore_;
  }

  void Start() {
    thread_.Start();
  }

  void Join() {
    semaphore_->Wait();
  }

  virtual void Run() = 0;

 private:
  class ThreadWithSemaphore : public i::Thread {
   public:
    explicit ThreadWithSemaphore(JoinableThread* joinable_thread)
      : Thread(joinable_thread->name_),
        joinable_thread_(joinable_thread) {
    }

    virtual void Run() {
      joinable_thread_->Run();
      joinable_thread_->semaphore_->Signal();
    }

   private:
    JoinableThread* joinable_thread_;
  };

  const char* name_;
  i::Semaphore* semaphore_;
  ThreadWithSemaphore thread_;

  friend class ThreadWithSemaphore;

  DISALLOW_COPY_AND_ASSIGN(JoinableThread);
};


class IsolateLockingThreadWithLocalContext : public JoinableThread {
 public:
  explicit IsolateLockingThreadWithLocalContext(v8::Isolate* isolate)
    : JoinableThread("IsolateLockingThread"),
      isolate_(isolate) {
  }

  virtual void Run() {
    v8::Locker locker(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope;
    LocalContext local_context;
    CHECK_EQ(isolate_, v8::internal::Isolate::Current());
    CalcFibAndCheck();
  }
 private:
  v8::Isolate* isolate_;
};

static void StartJoinAndDeleteThreads(const i::List<JoinableThread*>& threads) {
  for (int i = 0; i < threads.length(); i++) {
    threads[i]->Start();
  }
  for (int i = 0; i < threads.length(); i++) {
    threads[i]->Join();
  }
  for (int i = 0; i < threads.length(); i++) {
    delete threads[i];
  }
}


// Run many threads all locking on the same isolate
TEST(IsolateLockingStress) {
#ifdef V8_TARGET_ARCH_MIPS
  const int kNThreads = 50;
#else
  const int kNThreads = 100;
#endif
  i::List<JoinableThread*> threads(kNThreads);
  v8::Isolate* isolate = v8::Isolate::New();
  for (int i = 0; i < kNThreads; i++) {
    threads.Add(new IsolateLockingThreadWithLocalContext(isolate));
  }
  StartJoinAndDeleteThreads(threads);
  isolate->Dispose();
}

class IsolateNonlockingThread : public JoinableThread {
 public:
  explicit IsolateNonlockingThread()
    : JoinableThread("IsolateNonlockingThread") {
  }

  virtual void Run() {
    v8::Isolate* isolate = v8::Isolate::New();
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope;
      v8::Handle<v8::Context> context = v8::Context::New();
      v8::Context::Scope context_scope(context);
      CHECK_EQ(isolate, v8::internal::Isolate::Current());
      CalcFibAndCheck();
    }
    isolate->Dispose();
  }
 private:
};

// Run many threads each accessing its own isolate without locking
TEST(MultithreadedParallelIsolates) {
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_MIPS)
  const int kNThreads = 10;
#else
  const int kNThreads = 50;
#endif
  i::List<JoinableThread*> threads(kNThreads);
  for (int i = 0; i < kNThreads; i++) {
    threads.Add(new IsolateNonlockingThread());
  }
  StartJoinAndDeleteThreads(threads);
}


class IsolateNestedLockingThread : public JoinableThread {
 public:
  explicit IsolateNestedLockingThread(v8::Isolate* isolate)
    : JoinableThread("IsolateNestedLocking"), isolate_(isolate) {
  }
  virtual void Run() {
    v8::Locker lock(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope;
    LocalContext local_context;
    {
      v8::Locker another_lock(isolate_);
      CalcFibAndCheck();
    }
    {
      v8::Locker another_lock(isolate_);
      CalcFibAndCheck();
    }
  }
 private:
  v8::Isolate* isolate_;
};

// Run  many threads with nested locks
TEST(IsolateNestedLocking) {
#ifdef V8_TARGET_ARCH_MIPS
  const int kNThreads = 50;
#else
  const int kNThreads = 100;
#endif
  v8::Isolate* isolate = v8::Isolate::New();
  i::List<JoinableThread*> threads(kNThreads);
  for (int i = 0; i < kNThreads; i++) {
    threads.Add(new IsolateNestedLockingThread(isolate));
  }
  StartJoinAndDeleteThreads(threads);
}


class SeparateIsolatesLocksNonexclusiveThread : public JoinableThread {
 public:
  SeparateIsolatesLocksNonexclusiveThread(v8::Isolate* isolate1,
                                          v8::Isolate* isolate2)
    : JoinableThread("SeparateIsolatesLocksNonexclusiveThread"),
      isolate1_(isolate1), isolate2_(isolate2) {
  }

  virtual void Run() {
    v8::Locker lock(isolate1_);
    v8::Isolate::Scope isolate_scope(isolate1_);
    v8::HandleScope handle_scope;
    LocalContext local_context;

    IsolateLockingThreadWithLocalContext threadB(isolate2_);
    threadB.Start();
    CalcFibAndCheck();
    threadB.Join();
  }
 private:
  v8::Isolate* isolate1_;
  v8::Isolate* isolate2_;
};

// Run parallel threads that lock and access different isolates in parallel
TEST(SeparateIsolatesLocksNonexclusive) {
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_MIPS)
  const int kNThreads = 50;
#else
  const int kNThreads = 100;
#endif
  v8::Isolate* isolate1 = v8::Isolate::New();
  v8::Isolate* isolate2 = v8::Isolate::New();
  i::List<JoinableThread*> threads(kNThreads);
  for (int i = 0; i < kNThreads; i++) {
    threads.Add(new SeparateIsolatesLocksNonexclusiveThread(isolate1,
                                                             isolate2));
  }
  StartJoinAndDeleteThreads(threads);
  isolate2->Dispose();
  isolate1->Dispose();
}

class LockIsolateAndCalculateFibSharedContextThread : public JoinableThread {
 public:
  explicit LockIsolateAndCalculateFibSharedContextThread(
      v8::Isolate* isolate, v8::Handle<v8::Context> context)
    : JoinableThread("LockIsolateAndCalculateFibThread"),
      isolate_(isolate),
      context_(context) {
  }

  virtual void Run() {
    v8::Locker lock(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope;
    v8::Context::Scope context_scope(context_);
    CalcFibAndCheck();
  }
 private:
  v8::Isolate* isolate_;
  Persistent<v8::Context> context_;
};

class LockerUnlockerThread : public JoinableThread {
 public:
  explicit LockerUnlockerThread(v8::Isolate* isolate)
    : JoinableThread("LockerUnlockerThread"),
      isolate_(isolate) {
  }

  virtual void Run() {
    v8::Locker lock(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = v8::Context::New();
    {
      v8::Context::Scope context_scope(context);
      CalcFibAndCheck();
    }
    {
      isolate_->Exit();
      v8::Unlocker unlocker(isolate_);
      LockIsolateAndCalculateFibSharedContextThread thread(isolate_, context);
      thread.Start();
      thread.Join();
    }
    isolate_->Enter();
    {
      v8::Context::Scope context_scope(context);
      CalcFibAndCheck();
    }
  }

 private:
  v8::Isolate* isolate_;
};

// Use unlocker inside of a Locker, multiple threads.
TEST(LockerUnlocker) {
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_MIPS)
  const int kNThreads = 50;
#else
  const int kNThreads = 100;
#endif
  i::List<JoinableThread*> threads(kNThreads);
  v8::Isolate* isolate = v8::Isolate::New();
  for (int i = 0; i < kNThreads; i++) {
    threads.Add(new LockerUnlockerThread(isolate));
  }
  StartJoinAndDeleteThreads(threads);
  isolate->Dispose();
}

class LockTwiceAndUnlockThread : public JoinableThread {
 public:
  explicit LockTwiceAndUnlockThread(v8::Isolate* isolate)
    : JoinableThread("LockTwiceAndUnlockThread"),
      isolate_(isolate) {
  }

  virtual void Run() {
    v8::Locker lock(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = v8::Context::New();
    {
      v8::Context::Scope context_scope(context);
      CalcFibAndCheck();
    }
    {
      v8::Locker second_lock(isolate_);
      {
        isolate_->Exit();
        v8::Unlocker unlocker(isolate_);
        LockIsolateAndCalculateFibSharedContextThread thread(isolate_, context);
        thread.Start();
        thread.Join();
      }
    }
    isolate_->Enter();
    {
      v8::Context::Scope context_scope(context);
      CalcFibAndCheck();
    }
  }

 private:
  v8::Isolate* isolate_;
};

// Use Unlocker inside two Lockers.
TEST(LockTwiceAndUnlock) {
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_MIPS)
  const int kNThreads = 50;
#else
  const int kNThreads = 100;
#endif
  i::List<JoinableThread*> threads(kNThreads);
  v8::Isolate* isolate = v8::Isolate::New();
  for (int i = 0; i < kNThreads; i++) {
    threads.Add(new LockTwiceAndUnlockThread(isolate));
  }
  StartJoinAndDeleteThreads(threads);
  isolate->Dispose();
}

class LockAndUnlockDifferentIsolatesThread : public JoinableThread {
 public:
  LockAndUnlockDifferentIsolatesThread(v8::Isolate* isolate1,
                                       v8::Isolate* isolate2)
    : JoinableThread("LockAndUnlockDifferentIsolatesThread"),
      isolate1_(isolate1),
      isolate2_(isolate2) {
  }

  virtual void Run() {
    Persistent<v8::Context> context1;
    Persistent<v8::Context> context2;
    v8::Locker lock1(isolate1_);
    CHECK(v8::Locker::IsLocked(isolate1_));
    CHECK(!v8::Locker::IsLocked(isolate2_));
    {
      v8::Isolate::Scope isolate_scope(isolate1_);
      v8::HandleScope handle_scope;
      context1 = v8::Context::New();
      {
        v8::Context::Scope context_scope(context1);
        CalcFibAndCheck();
      }
    }
    v8::Locker lock2(isolate2_);
    CHECK(v8::Locker::IsLocked(isolate1_));
    CHECK(v8::Locker::IsLocked(isolate2_));
    {
      v8::Isolate::Scope isolate_scope(isolate2_);
      v8::HandleScope handle_scope;
      context2 = v8::Context::New();
      {
        v8::Context::Scope context_scope(context2);
        CalcFibAndCheck();
      }
    }
    {
      v8::Unlocker unlock1(isolate1_);
      CHECK(!v8::Locker::IsLocked(isolate1_));
      CHECK(v8::Locker::IsLocked(isolate2_));
      v8::Isolate::Scope isolate_scope(isolate2_);
      v8::HandleScope handle_scope;
      v8::Context::Scope context_scope(context2);
      LockIsolateAndCalculateFibSharedContextThread thread(isolate1_, context1);
      thread.Start();
      CalcFibAndCheck();
      thread.Join();
    }
  }

 private:
  v8::Isolate* isolate1_;
  v8::Isolate* isolate2_;
};

// Lock two isolates and unlock one of them.
TEST(LockAndUnlockDifferentIsolates) {
  v8::Isolate* isolate1 = v8::Isolate::New();
  v8::Isolate* isolate2 = v8::Isolate::New();
  LockAndUnlockDifferentIsolatesThread thread(isolate1, isolate2);
  thread.Start();
  thread.Join();
  isolate2->Dispose();
  isolate1->Dispose();
}

class LockUnlockLockThread : public JoinableThread {
 public:
  LockUnlockLockThread(v8::Isolate* isolate, v8::Handle<v8::Context> context)
    : JoinableThread("LockUnlockLockThread"),
      isolate_(isolate),
      context_(context) {
  }

  virtual void Run() {
    v8::Locker lock1(isolate_);
    CHECK(v8::Locker::IsLocked(isolate_));
    CHECK(!v8::Locker::IsLocked());
    {
      v8::Isolate::Scope isolate_scope(isolate_);
      v8::HandleScope handle_scope;
      v8::Context::Scope context_scope(context_);
      CalcFibAndCheck();
    }
    {
      v8::Unlocker unlock1(isolate_);
      CHECK(!v8::Locker::IsLocked(isolate_));
      CHECK(!v8::Locker::IsLocked());
      {
        v8::Locker lock2(isolate_);
        v8::Isolate::Scope isolate_scope(isolate_);
        v8::HandleScope handle_scope;
        CHECK(v8::Locker::IsLocked(isolate_));
        CHECK(!v8::Locker::IsLocked());
        v8::Context::Scope context_scope(context_);
        CalcFibAndCheck();
      }
    }
  }

 private:
  v8::Isolate* isolate_;
  v8::Persistent<v8::Context> context_;
};

// Locker inside an Unlocker inside a Locker.
TEST(LockUnlockLockMultithreaded) {
#ifdef V8_TARGET_ARCH_MIPS
  const int kNThreads = 50;
#else
  const int kNThreads = 100;
#endif
  v8::Isolate* isolate = v8::Isolate::New();
  Persistent<v8::Context> context;
  {
    v8::Locker locker_(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope;
    context = v8::Context::New();
  }
  i::List<JoinableThread*> threads(kNThreads);
  for (int i = 0; i < kNThreads; i++) {
    threads.Add(new LockUnlockLockThread(isolate, context));
  }
  StartJoinAndDeleteThreads(threads);
}

class LockUnlockLockDefaultIsolateThread : public JoinableThread {
 public:
  explicit LockUnlockLockDefaultIsolateThread(v8::Handle<v8::Context> context)
    : JoinableThread("LockUnlockLockDefaultIsolateThread"),
      context_(context) {
  }

  virtual void Run() {
    v8::Locker lock1;
    {
      v8::HandleScope handle_scope;
      v8::Context::Scope context_scope(context_);
      CalcFibAndCheck();
    }
    {
      v8::Unlocker unlock1;
      {
        v8::Locker lock2;
        v8::HandleScope handle_scope;
        v8::Context::Scope context_scope(context_);
        CalcFibAndCheck();
      }
    }
  }

 private:
  v8::Persistent<v8::Context> context_;
};

// Locker inside an Unlocker inside a Locker for default isolate.
TEST(LockUnlockLockDefaultIsolateMultithreaded) {
#ifdef V8_TARGET_ARCH_MIPS
  const int kNThreads = 50;
#else
  const int kNThreads = 100;
#endif
  Persistent<v8::Context> context;
  {
    v8::Locker locker_;
    v8::HandleScope handle_scope;
    context = v8::Context::New();
  }
  i::List<JoinableThread*> threads(kNThreads);
  for (int i = 0; i < kNThreads; i++) {
    threads.Add(new LockUnlockLockDefaultIsolateThread(context));
  }
  StartJoinAndDeleteThreads(threads);
}


TEST(Regress1433) {
  for (int i = 0; i < 10; i++) {
    v8::Isolate* isolate = v8::Isolate::New();
    {
      v8::Locker lock(isolate);
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope;
      v8::Persistent<Context> context = v8::Context::New();
      v8::Context::Scope context_scope(context);
      v8::Handle<String> source = v8::String::New("1+1");
      v8::Handle<Script> script = v8::Script::Compile(source);
      v8::Handle<Value> result = script->Run();
      v8::String::AsciiValue ascii(result);
      context.Dispose();
    }
    isolate->Dispose();
  }
}


static const char* kSimpleExtensionSource =
  "(function Foo() {"
  "  return 4;"
  "})() ";

class IsolateGenesisThread : public JoinableThread {
 public:
  IsolateGenesisThread(int count, const char* extension_names[])
    : JoinableThread("IsolateGenesisThread"),
      count_(count),
      extension_names_(extension_names)
  {}

  virtual void Run() {
    v8::Isolate* isolate = v8::Isolate::New();
    {
      v8::Isolate::Scope isolate_scope(isolate);
      CHECK(!i::Isolate::Current()->has_installed_extensions());
      v8::ExtensionConfiguration extensions(count_, extension_names_);
      v8::Persistent<v8::Context> context = v8::Context::New(&extensions);
      CHECK(i::Isolate::Current()->has_installed_extensions());
      context.Dispose();
    }
    isolate->Dispose();
  }
 private:
  int count_;
  const char** extension_names_;
};

// Test installing extensions in separate isolates concurrently.
// http://code.google.com/p/v8/issues/detail?id=1821
TEST(ExtensionsRegistration) {
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_MIPS)
  const int kNThreads = 10;
#else
  const int kNThreads = 40;
#endif
  v8::RegisterExtension(new v8::Extension("test0",
                                          kSimpleExtensionSource));
  v8::RegisterExtension(new v8::Extension("test1",
                                          kSimpleExtensionSource));
  v8::RegisterExtension(new v8::Extension("test2",
                                          kSimpleExtensionSource));
  v8::RegisterExtension(new v8::Extension("test3",
                                          kSimpleExtensionSource));
  v8::RegisterExtension(new v8::Extension("test4",
                                          kSimpleExtensionSource));
  v8::RegisterExtension(new v8::Extension("test5",
                                          kSimpleExtensionSource));
  v8::RegisterExtension(new v8::Extension("test6",
                                          kSimpleExtensionSource));
  v8::RegisterExtension(new v8::Extension("test7",
                                          kSimpleExtensionSource));
  const char* extension_names[] = { "test0", "test1",
                                    "test2", "test3", "test4",
                                    "test5", "test6", "test7" };
  i::List<JoinableThread*> threads(kNThreads);
  for (int i = 0; i < kNThreads; i++) {
    threads.Add(new IsolateGenesisThread(8, extension_names));
  }
  StartJoinAndDeleteThreads(threads);
}
