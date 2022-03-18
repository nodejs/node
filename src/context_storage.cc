#include "context_storage.h"

#include <unordered_set>

namespace node {

std::unordered_set<std::shared_ptr<ContextStorage>> context_storages;
std::unordered_map<ContextStorage*, std::shared_ptr<ContextStorage>> context_storage_map;

ContextStorage::ContextStorage(v8::Isolate* isolate) : isolate_(isolate) {}
ContextStorage::ContextStorage() : isolate_(v8::Isolate::GetCurrent()) {}

std::shared_ptr<ContextStorage> ContextStorage::Create(v8::Isolate* isolate) {
  auto storage = std::make_shared<ContextStorage>(isolate);
  context_storage_map[storage.get()] = storage;
  return storage;
}
std::shared_ptr<ContextStorage> ContextStorage::Create() {
  auto storage = std::make_shared<ContextStorage>(v8::Isolate::GetCurrent());
  context_storage_map[storage.get()] = storage;
  return storage;
}

ContextStorage::~ContextStorage() {
  context_storage_map.extract(this);
  Disable();
}

void ContextStorage::Enter(const v8::PersistentBase<v8::Value>& data) {
  stored.Reset(isolate_, data);
}

bool ContextStorage::Enabled() {
  auto it = context_storages.find(context_storage_map[this]);
  return it != context_storages.end();
}

void ContextStorage::Enable() {
  context_storages.insert(context_storage_map[this]);
}

void ContextStorage::Disable() {
  context_storages.extract(context_storage_map[this]);
}

void ContextStorage::Enter(v8::Local<v8::Value> data) {
  stored.Reset(isolate_, data);
}

void ContextStorage::Exit() {
  stored.Reset();
}

v8::Local<v8::Value> ContextStorage::Get() {
  if (!Enabled() || stored.IsEmpty()) {
    return v8::Undefined(isolate_);
  }
  return stored.Get(isolate_);
}

void ContextStorageState::Restore() {
  for (auto storage : context_storages) {
    storage->Enter(state[storage]);
  }
}

void ContextStorageState::Clear() {
  for (auto storage : context_storages) {
    storage->Exit();
  }
}

std::shared_ptr<ContextStorageState> ContextStorageState::State() {
  std::shared_ptr<ContextStorageState> state =
      std::make_shared<ContextStorageState>();

  for (auto storage : context_storages) {
    state->state[storage].Reset(storage->isolate_, storage->stored);
  }

  return state;
}

ContextStorageRunScope::ContextStorageRunScope(std::shared_ptr<ContextStorage> storage,
                                               v8::Local<v8::Value> data)
    : storage_(storage) {
  storage_->Enable();
  storage_->Enter(data);
}

ContextStorageRunScope::~ContextStorageRunScope() {
  storage_->Exit();
}

ContextStorageExitScope::ContextStorageExitScope(std::shared_ptr<ContextStorage> storage)
    : storage_(storage) {
  storage_->Disable();
}

ContextStorageExitScope::~ContextStorageExitScope() {
  storage_->Enable();
}

ContextStorageRestoreScope::ContextStorageRestoreScope(
    std::shared_ptr<ContextStorageState> state)
    : state(std::move(state)) {
  if (!state) return;
  state->Restore();
}

ContextStorageRestoreScope::~ContextStorageRestoreScope() {
  if (!state) return;
  state->Clear();
}

}  // namespace node
