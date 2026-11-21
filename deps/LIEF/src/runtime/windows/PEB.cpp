/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "LIEF/runtime/windows/PEB.hpp"
#include "LIEF/runtime/windows/Process.hpp"

#include <cstddef>
#include <cstdint>

#include <Windows.h>

namespace LIEF::runtime::windows {
namespace details {

template<class T>
struct peb_layout {
  uint8_t Reserved1[2];
  uint8_t BeingDebugged;
  uint8_t Reserved2[1];
  T Reserved3[2];
  T Ldr;
  T ProcessParameters;
  T Reserved4[3];
  T AtlThunkSListPtr;
  T Reserved5;
  uint32_t Reserved6;
  T Reserved7;
  uint32_t Reserved8;
  uint32_t AtlThunkSListPtr32;
  T Reserved9[45];
  uint8_t Reserved10[96];
  T PostProcessInitRoutine;
  uint8_t Reserved11[128];
  T Reserved12[1];
  uint32_t SessionId;
};

static_assert(offsetof(peb_layout<uint64_t>, BeingDebugged) == 0x02,
              "Invalid layout");
static_assert(offsetof(peb_layout<uint64_t>, SessionId) == 0x2C0,
              "Invalid layout");
static_assert(offsetof(peb_layout<uint32_t>, BeingDebugged) == 0x02,
              "Invalid layout");
static_assert(offsetof(peb_layout<uint32_t>, SessionId) == 0x1D4,
              "Invalid layout");
using peb_layout_t = peb_layout<uintptr_t>;

class peb {
  public:
  explicit peb(const peb_layout_t* raw) :
    raw_(raw) {}

  const peb_layout_t& raw() const {
    return *raw_;
  }

  private:
  const peb_layout_t* raw_ = nullptr;
};

}

// Return the address of the PEB of the current process.
//
// The PEB pointer is stored within the TEB which
// can be accessed through a per-architecture mechanism abstracted by the
// `NtCurrentTeb()` intrinsic:
// - x86_64: `GS:[0x30]`
// - x86: `FS:[0x18]`
// - ARM64: `x18`
static uintptr_t current_peb() {
  constexpr uintptr_t PEB_OFFSET =
      sizeof(uintptr_t) == sizeof(uint64_t) ? 0x60 : 0x30;

  auto teb = reinterpret_cast<uintptr_t>(NtCurrentTeb());
  if (teb == 0) {
    return 0;
  }

  return *reinterpret_cast<const uintptr_t*>(teb + PEB_OFFSET);
}

std::unique_ptr<PEB> PEB::create() {
  const uintptr_t addr = current_peb();
  if (addr == 0) {
    return nullptr;
  }

  auto impl = std::make_unique<details::peb>(
      reinterpret_cast<const details::peb_layout_t*>(addr)
  );

  return std::unique_ptr<PEB>(new PEB(std::move(impl)));
}

bool PEB::being_debugged() const {
  return impl_->raw().BeingDebugged != 0;
}

uintptr_t PEB::ldr() const {
  return impl_->raw().Ldr;
}

uintptr_t PEB::process_parameters() const {
  return impl_->raw().ProcessParameters;
}

uintptr_t PEB::atl_thunk_slist_ptr() const {
  return impl_->raw().AtlThunkSListPtr;
}

uint32_t PEB::atl_thunk_slist_ptr32() const {
  return impl_->raw().AtlThunkSListPtr32;
}

uintptr_t PEB::post_process_init_routine() const {
  return impl_->raw().PostProcessInitRoutine;
}

uint32_t PEB::session_id() const {
  return impl_->raw().SessionId;
}

PEB::PEB(std::unique_ptr<details::peb> impl) :
  impl_(std::move(impl)) {}

PEB::PEB(PEB&&) noexcept = default;
PEB& PEB::operator=(PEB&&) noexcept = default;

PEB::~PEB() = default;
}
