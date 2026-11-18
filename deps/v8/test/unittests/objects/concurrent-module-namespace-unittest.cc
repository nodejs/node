// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

#include "src/api/api-inl.h"
#include "src/base/platform/semaphore.h"
#include "src/handles/handles-inl.h"
#include "src/handles/local-handles-inl.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/objects/module-inl.h"
#include "src/objects/prototype-info-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using ConcurrentModuleNamespaceTest = TestWithContext;

namespace {

constexpr int kNumIterations = 1000;

MaybeLocal<v8::Module> ResolveNothing(Local<v8::Context> context,
                                      Local<v8::String> specifier,
                                      Local<v8::FixedArray> import_attributes,
                                      Local<v8::Module> referrer) {
  return {};
}

Local<v8::Module> CompileModule(v8::Isolate* isolate, const char* source,
                                const char* name = "m.mjs") {
  ScriptOrigin origin(v8::String::NewFromUtf8(isolate, name).ToLocalChecked(),
                      0, 0, false, -1, Local<Value>(), false, false, true);
  ScriptCompiler::Source script_source(
      v8::String::NewFromUtf8(isolate, source).ToLocalChecked(), origin);
  return ScriptCompiler::CompileModule(isolate, &script_source)
      .ToLocalChecked();
}

Global<v8::Module>* dep_module = nullptr;

MaybeLocal<v8::Module> ResolveToDep(Local<v8::Context> context,
                                    Local<v8::String> specifier,
                                    Local<v8::FixedArray> import_attributes,
                                    Local<v8::Module> referrer) {
  return dep_module->Get(v8::Isolate::GetCurrent());
}

// Mirrors what the compiler does on a background thread to find the
// namespace behind a namespace map.
class NamespaceBackPointerThread final : public v8::base::Thread {
 public:
  NamespaceBackPointerThread(Heap* heap, IndirectHandle<Map> map,
                             IndirectHandle<JSModuleNamespace> ns,
                             std::unique_ptr<PersistentHandles> ph,
                             base::Semaphore* sema_started)
      : v8::base::Thread(base::Thread::Options("NamespaceBackPointerThread")),
        heap_(heap),
        map_(map),
        ns_(ns),
        ph_(std::move(ph)),
        sema_started_(sema_started) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground, std::move(ph_));
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);

    sema_started_->Signal();

    for (int i = 0; i < kNumIterations; i++) {
      // Both the map the job started with (which may have lost its
      // PrototypeInfo) and the namespace's current map.
      Check(*map_);
      Check(ns_->map(kAcquireLoad));
      local_heap.Safepoint();
    }
  }

 private:
  void Check(Tagged<Map> map) {
    Tagged<Object> maybe_info = map->prototype_info(kAcquireLoad);
    Tagged<PrototypeInfo> info;
    if (!TryCast<PrototypeInfo>(maybe_info, &info)) return;
    Tagged<Object> ns = info->module_namespace(kAcquireLoad);
    CHECK_EQ(ns, *ns_);
  }

  Heap* heap_;
  IndirectHandle<Map> map_;
  IndirectHandle<JSModuleNamespace> ns_;
  std::unique_ptr<PersistentHandles> ph_;
  base::Semaphore* sema_started_;
};

class ModuleStatusThread final : public v8::base::Thread {
 public:
  ModuleStatusThread(Heap* heap, IndirectHandle<Module> module,
                     std::unique_ptr<PersistentHandles> ph,
                     base::Semaphore* sema_started, std::atomic<bool>* done)
      : v8::base::Thread(base::Thread::Options("ModuleStatusThread")),
        heap_(heap),
        module_(module),
        ph_(std::move(ph)),
        sema_started_(sema_started),
        done_(done) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground, std::move(ph_));
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);

    sema_started_->Signal();

    Module::Status last = module_->status(kRelaxedLoad);
    for (int i = 0; i < kNumIterations || !done_->load(); i++) {
      Module::Status current = module_->status(kRelaxedLoad);
      CHECK_GE(current, last);
      last = current;
      local_heap.Safepoint();
    }
    CHECK_EQ(module_->status(kRelaxedLoad), Module::kEvaluated);
  }

 private:
  Heap* heap_;
  IndirectHandle<Module> module_;
  std::unique_ptr<PersistentHandles> ph_;
  base::Semaphore* sema_started_;
  std::atomic<bool>* done_;
};

// Mirrors ModuleRef::LookupExportCell: once the exports are finalized, the
// star-exported name must be in the table and resolved to a cell.
class ExportsFinalizedThread final : public v8::base::Thread {
 public:
  ExportsFinalizedThread(Heap* heap, IndirectHandle<Module> module,
                         IndirectHandle<Name> name,
                         std::unique_ptr<PersistentHandles> ph,
                         base::Semaphore* sema_started, std::atomic<bool>* done)
      : v8::base::Thread(base::Thread::Options("ExportsFinalizedThread")),
        heap_(heap),
        module_(module),
        name_(name),
        ph_(std::move(ph)),
        sema_started_(sema_started),
        done_(done) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground, std::move(ph_));
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);

    sema_started_->Signal();

    bool seen_finalized = false;
    for (int i = 0; i < kNumIterations || !done_->load(); i++) {
      if (module_->ExportsFinalized(kAcquireLoad)) {
        seen_finalized = true;
        Tagged<Object> entry = module_->exports()->Lookup(
            name_, Smi::ToInt(Object::GetHash(*name_)));
        CHECK(IsCell(entry));
      } else {
        // Finalization is never undone.
        CHECK(!seen_finalized);
      }
      local_heap.Safepoint();
    }
    CHECK(module_->ExportsFinalized(kAcquireLoad));
  }

 private:
  Heap* heap_;
  IndirectHandle<Module> module_;
  IndirectHandle<Name> name_;
  std::unique_ptr<PersistentHandles> ph_;
  base::Semaphore* sema_started_;
  std::atomic<bool>* done_;
};

}  // namespace

// The main thread migrates the namespace's (prototype) map, which moves the
// PrototypeInfo to the new map and leaves Smi::zero() in the old one, while a
// background thread follows the map -> PrototypeInfo -> namespace back
// pointer.
// TODO(caiolima): Once --js-nonextensible-applies-to-private ships, we might
// be able to assume that namespace maps never migrate. If so, remove this
// test.
TEST_F(ConcurrentModuleNamespaceTest, ReadNamespaceBackPointer_MapMigration) {
  Isolate* isolate = i_isolate();
  HandleScope handle_scope(isolate);

  Local<v8::Module> api_module =
      CompileModule(v8_isolate(), "export let a = 1; export let b = 2;");
  CHECK(api_module->InstantiateModule(context(), ResolveNothing).FromJust());
  api_module->Evaluate(context()).ToLocalChecked();

  Handle<Module> module = Cast<Module>(Utils::OpenHandle(*api_module));
  DirectHandle<JSModuleNamespace> ns =
      Module::GetModuleNamespace(isolate, module);
  CHECK(ns->map()->is_prototype_map());
  CHECK(ns->map()->is_dictionary_map());
  CHECK_EQ(Cast<PrototypeInfo>(ns->map()->prototype_info())->module_namespace(),
           *ns);

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  IndirectHandle<Map> map = ph->NewHandle(ns->map());
  IndirectHandle<JSModuleNamespace> persistent_ns = ph->NewHandle(*ns);

  base::Semaphore sema_started(0);
  std::unique_ptr<NamespaceBackPointerThread> thread(
      new NamespaceBackPointerThread(isolate->heap(), map, persistent_ns,
                                     std::move(ph), &sema_started));
  CHECK(thread->Start());
  sema_started.Wait();

  for (int i = 0; i < 100; i++) {
    HandleScope inner_scope(isolate);
    DirectHandle<Map> old_map(ns->map(), isolate);
    // Alternate between the slow-to-fast migration that ICs trigger through
    // MakePrototypesFast and normalizing back to dictionary mode.
    if (old_map->is_dictionary_map()) {
      JSObject::MigrateSlowToFast(ns, 0, "test");
    } else {
      JSObject::NormalizeProperties(isolate, ns, KEEP_INOBJECT_PROPERTIES, 0,
                                    /*use_cache=*/false, "test");
    }
    CHECK_NE(ns->map(), *old_map);
    CHECK(ns->map()->is_prototype_map());
    CHECK_EQ(old_map->prototype_info(), Smi::zero());
    CHECK_EQ(
        Cast<PrototypeInfo>(ns->map()->prototype_info())->module_namespace(),
        *ns);
  }

  thread->Join();
}

// The main thread links and evaluates a module while a background thread
// reads its status.
TEST_F(ConcurrentModuleNamespaceTest, ReadModuleStatus_WhileEvaluating) {
  Isolate* isolate = i_isolate();
  HandleScope handle_scope(isolate);

  Local<v8::Module> api_module = CompileModule(
      v8_isolate(), "export let a = 0; for (let i = 0; i < 1000; i++) a += i;");
  Handle<Module> module = Cast<Module>(Utils::OpenHandle(*api_module));
  CHECK_EQ(module->status(), Module::kUnlinked);

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  IndirectHandle<Module> persistent_module = ph->NewHandle(*module);

  base::Semaphore sema_started(0);
  std::atomic<bool> done{false};
  std::unique_ptr<ModuleStatusThread> thread(new ModuleStatusThread(
      isolate->heap(), persistent_module, std::move(ph), &sema_started, &done));
  CHECK(thread->Start());
  sema_started.Wait();

  CHECK(api_module->InstantiateModule(context(), ResolveNothing).FromJust());
  api_module->Evaluate(context()).ToLocalChecked();
  CHECK_EQ(module->status(), Module::kEvaluated);
  done.store(true);

  thread->Join();
}

// The main thread creates a module's first namespace, which fetches its star
// exports into the exports table, while a background thread checks whether the
// exports are finalized and, if so, reads the table.
TEST_F(ConcurrentModuleNamespaceTest, ReadExports_WhileCreatingNamespace) {
  Isolate* isolate = i_isolate();
  HandleScope handle_scope(isolate);

  Global<v8::Module> dep(
      v8_isolate(),
      CompileModule(v8_isolate(), "export let z = 2;", "dep.mjs"));
  dep_module = &dep;
  Local<v8::Module> api_module = CompileModule(
      v8_isolate(), "export let a = 1; export * from 'dep.mjs';", "main.mjs");
  CHECK(api_module->InstantiateModule(context(), ResolveToDep).FromJust());
  api_module->Evaluate(context()).ToLocalChecked();
  dep_module = nullptr;

  Handle<Module> module = Cast<Module>(Utils::OpenHandle(*api_module));
  DirectHandle<String> z = isolate->factory()->InternalizeUtf8String("z");
  CHECK(!module->ExportsFinalized(kAcquireLoad));
  CHECK(IsTheHole(module->exports()->Lookup(z)));

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  IndirectHandle<Module> persistent_module = ph->NewHandle(*module);
  IndirectHandle<Name> persistent_name = ph->NewHandle(*z);

  base::Semaphore sema_started(0);
  std::atomic<bool> done{false};
  std::unique_ptr<ExportsFinalizedThread> thread(new ExportsFinalizedThread(
      isolate->heap(), persistent_module, persistent_name, std::move(ph),
      &sema_started, &done));
  CHECK(thread->Start());
  sema_started.Wait();

  Module::GetModuleNamespace(isolate, module);
  CHECK(module->ExportsFinalized(kAcquireLoad));
  CHECK(IsCell(module->exports()->Lookup(z)));
  done.store(true);

  thread->Join();
}

}  // namespace internal
}  // namespace v8
