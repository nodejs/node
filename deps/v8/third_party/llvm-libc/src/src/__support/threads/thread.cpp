//===--- Definitions of common thread items ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/threads/thread.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/mutex.h"

#include "src/__support/CPP/array.h"
#include "src/__support/CPP/mutex.h" // lock_guard
#include "src/__support/CPP/optional.h"
#include "src/__support/fixedvector.h"
#include "src/__support/macros/attributes.h"

namespace LIBC_NAMESPACE_DECL {
namespace {

using AtExitCallback = void(void *);

struct AtExitUnit {
  AtExitCallback *callback = nullptr;
  void *obj = nullptr;
  constexpr AtExitUnit() = default;
  constexpr AtExitUnit(AtExitCallback *cb, void *o) : callback(cb), obj(o) {}
};

constexpr size_t TSS_KEY_COUNT = 1024;

struct TSSKeyUnit {
  // Indicates whether is unit is active. Presence of a non-null dtor
  // is not sufficient to indicate the same information as a TSS key can
  // have a null destructor.
  bool active = false;

  TSSDtor *dtor = nullptr;

  constexpr TSSKeyUnit() = default;
  constexpr TSSKeyUnit(TSSDtor *d) : active(true), dtor(d) {}

  void reset() {
    active = false;
    dtor = nullptr;
  }
};

class TSSKeyMgr {
  Mutex mtx;
  cpp::array<TSSKeyUnit, TSS_KEY_COUNT> units;

public:
  constexpr TSSKeyMgr()
      : mtx(/*is_priority_inherit=*/false, /*is_recursive=*/false,
            /*is_robust=*/false, /*is_pshared=*/false) {}

  cpp::optional<unsigned int> new_key(TSSDtor *dtor) {
    cpp::lock_guard lock(mtx);
    for (unsigned int i = 0; i < TSS_KEY_COUNT; ++i) {
      TSSKeyUnit &u = units[i];
      if (!u.active) {
        u = {dtor};
        return i;
      }
    }
    return cpp::optional<unsigned int>();
  }

  TSSDtor *get_dtor(unsigned int key) {
    if (key >= TSS_KEY_COUNT)
      return nullptr;
    cpp::lock_guard lock(mtx);
    return units[key].dtor;
  }

  bool remove_key(unsigned int key) {
    if (key >= TSS_KEY_COUNT)
      return false;
    cpp::lock_guard lock(mtx);
    units[key].reset();
    return true;
  }

  bool is_valid_key(unsigned int key) {
    cpp::lock_guard lock(mtx);
    return units[key].active;
  }
};

TSSKeyMgr tss_key_mgr;

struct TSSValueUnit {
  bool active = false;
  void *payload = nullptr;
  TSSDtor *dtor = nullptr;

  constexpr TSSValueUnit() = default;
  constexpr TSSValueUnit(void *p, TSSDtor *d)
      : active(true), payload(p), dtor(d) {}
};

static LIBC_THREAD_LOCAL cpp::array<TSSValueUnit, TSS_KEY_COUNT> tss_values;

} // anonymous namespace

class ThreadAtExitCallbackMgr {
  Mutex mtx;
  // TODO: Use a BlockStore when compiled for production.
  FixedVector<AtExitUnit, 1024> callback_list;

public:
  constexpr ThreadAtExitCallbackMgr()
      : mtx(/*is_priority_inherit=*/false, /*is_recursive=*/false,
            /*is_robust=*/false, /*is_pshared=*/false) {}

  int add_callback(AtExitCallback *callback, void *obj) {
    cpp::lock_guard lock(mtx);
    if (callback_list.push_back({callback, obj}))
      return 0;
    return -1;
  }

  void call() {
    mtx.lock();
    while (!callback_list.empty()) {
      auto atexit_unit = callback_list.back();
      callback_list.pop_back();
      mtx.unlock();
      atexit_unit.callback(atexit_unit.obj);
      mtx.lock();
    }
  }
};

static LIBC_THREAD_LOCAL ThreadAtExitCallbackMgr atexit_callback_mgr;

// The function __cxa_thread_atexit is provided by C++ runtimes like libcxxabi.
// It is used by thread local object runtime to register destructor calls. To
// actually register destructor call with the threading library, it calls
// __cxa_thread_atexit_impl, which is to be provided by the threading library.
// The semantics are very similar to the __cxa_atexit function except for the
// fact that the registered callback is thread specific.
extern "C" int __cxa_thread_atexit_impl(AtExitCallback *callback, void *obj,
                                        void *) {
  return atexit_callback_mgr.add_callback(callback, obj);
}

namespace internal {

ThreadAtExitCallbackMgr *get_thread_atexit_callback_mgr() {
  return &atexit_callback_mgr;
}

void call_atexit_callbacks(ThreadAttributes *attrib) {
  attrib->atexit_callback_mgr->call();
  for (size_t i = 0; i < TSS_KEY_COUNT; ++i) {
    TSSValueUnit &unit = tss_values[i];
    // Both dtor and value need to nonnull to call dtor
    if (unit.dtor != nullptr && unit.payload != nullptr)
      unit.dtor(unit.payload);
  }
}

extern "C" void __cxa_thread_finalize() { call_atexit_callbacks(self.attrib); }

} // namespace internal

cpp::optional<unsigned int> new_tss_key(TSSDtor *dtor) {
  return tss_key_mgr.new_key(dtor);
}

bool tss_key_delete(unsigned int key) { return tss_key_mgr.remove_key(key); }

bool set_tss_value(unsigned int key, void *val) {
  if (!tss_key_mgr.is_valid_key(key))
    return false;
  tss_values[key] = {val, tss_key_mgr.get_dtor(key)};
  return true;
}

void *get_tss_value(unsigned int key) {
  if (key >= TSS_KEY_COUNT)
    return nullptr;

  auto &u = tss_values[key];
  if (!u.active)
    return nullptr;
  return u.payload;
}

} // namespace LIBC_NAMESPACE_DECL
