/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
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
#include "LIEF/Visitor.hpp"
#include "LIEF/MachO/ChainedBindingInfo.hpp"
#include "MachO/ChainedFixup.hpp"

namespace LIEF {
namespace MachO {

ChainedBindingInfo::ChainedBindingInfo(ChainedBindingInfo&&) noexcept = default;
ChainedBindingInfo& ChainedBindingInfo::operator=(ChainedBindingInfo&&) noexcept = default;

ChainedBindingInfo::ChainedBindingInfo(const ChainedBindingInfo& other) :
  BindingInfo(other),
  format_{other.format_},
  ptr_format_{other.ptr_format_},
  offset_{other.offset_},
  btypes_{other.btypes_}
{

  switch (btypes_) {
    case BIND_TYPES::ARM64E_AUTH_BIND:   arm64_auth_bind_    = new details::dyld_chained_ptr_arm64e_auth_bind{*other.arm64_auth_bind_};      break;
    case BIND_TYPES::ARM64E_BIND:        arm64_bind_         = new details::dyld_chained_ptr_arm64e_bind{*other.arm64_bind_};                break;
    case BIND_TYPES::ARM64E_BIND24:      arm64_bind24_       = new details::dyld_chained_ptr_arm64e_bind24{*other.arm64_bind24_};            break;
    case BIND_TYPES::ARM64E_AUTH_BIND24: arm64_auth_bind24_  = new details::dyld_chained_ptr_arm64e_auth_bind24{*other.arm64_auth_bind24_};  break;
    case BIND_TYPES::PTR32_BIND:         p32_bind_           = new details::dyld_chained_ptr_32_bind{*other.p32_bind_};                      break;
    case BIND_TYPES::PTR64_BIND:         p64_bind_           = new details::dyld_chained_ptr_64_bind{*other.p64_bind_};                      break;
    case BIND_TYPES::UNKNOWN: {}
  }
}

ChainedBindingInfo::ChainedBindingInfo(DYLD_CHAINED_FORMAT fmt, bool is_weak) :
  format_{fmt}
{
  is_weak_import_ = is_weak;
}

ChainedBindingInfo::~ChainedBindingInfo() {
  clear();
}

ChainedBindingInfo& ChainedBindingInfo::operator=(ChainedBindingInfo other) {
  swap(other);
  return *this;
}

void ChainedBindingInfo::swap(ChainedBindingInfo& other) noexcept {
  BindingInfo::swap(other);
  std::swap(format_,          other.format_);
  std::swap(ptr_format_,      other.ptr_format_);
  std::swap(offset_,          other.offset_);
  std::swap(btypes_,          other.btypes_);
}

void ChainedBindingInfo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

uint64_t ChainedBindingInfo::sign_extended_addend() const {
  switch (btypes_) {
    case BIND_TYPES::ARM64E_AUTH_BIND:   return 0;
    case BIND_TYPES::ARM64E_BIND:        return details::sign_extended_addend(*arm64_bind_);
    case BIND_TYPES::ARM64E_BIND24:      return details::sign_extended_addend(*arm64_bind24_);
    case BIND_TYPES::ARM64E_AUTH_BIND24: return 0;
    case BIND_TYPES::PTR32_BIND:         return p32_bind_->addend;
    case BIND_TYPES::PTR64_BIND:         return details::sign_extended_addend(*p64_bind_);
    case BIND_TYPES::UNKNOWN:            return 0;
  }
  return 0;
}


void ChainedBindingInfo::set(const details::dyld_chained_ptr_arm64e_bind& bind) {
  clear();
  btypes_ = BIND_TYPES::ARM64E_BIND;
  arm64_bind_ = new details::dyld_chained_ptr_arm64e_bind(bind);
}

void ChainedBindingInfo::set(const details::dyld_chained_ptr_arm64e_auth_bind& bind) {
  clear();
  btypes_ = BIND_TYPES::ARM64E_AUTH_BIND;
  arm64_auth_bind_ = new details::dyld_chained_ptr_arm64e_auth_bind(bind);
}

void ChainedBindingInfo::set(const details::dyld_chained_ptr_arm64e_bind24& bind) {
  clear();
  btypes_ = BIND_TYPES::ARM64E_BIND24;
  arm64_bind24_ = new details::dyld_chained_ptr_arm64e_bind24(bind);
}

void ChainedBindingInfo::set(const details::dyld_chained_ptr_arm64e_auth_bind24& bind) {
  clear();
  btypes_ = BIND_TYPES::ARM64E_AUTH_BIND24;
  arm64_auth_bind24_ = new details::dyld_chained_ptr_arm64e_auth_bind24(bind);
}

void ChainedBindingInfo::set(const details::dyld_chained_ptr_64_bind& bind) {
  clear();
  btypes_ = BIND_TYPES::PTR64_BIND;
  p64_bind_ = new details::dyld_chained_ptr_64_bind(bind);
}

void ChainedBindingInfo::set(const details::dyld_chained_ptr_32_bind& bind) {
  clear();
  btypes_ = BIND_TYPES::PTR32_BIND;
  p32_bind_ = new details::dyld_chained_ptr_32_bind(bind);
}


void ChainedBindingInfo::clear() {
  switch (btypes_) {
    case BIND_TYPES::ARM64E_AUTH_BIND:   delete arm64_auth_bind_;   break;
    case BIND_TYPES::ARM64E_BIND:        delete arm64_bind_;        break;
    case BIND_TYPES::ARM64E_BIND24:      delete arm64_bind24_;      break;
    case BIND_TYPES::ARM64E_AUTH_BIND24: delete arm64_auth_bind24_; break;
    case BIND_TYPES::PTR32_BIND:         delete p32_bind_;          break;
    case BIND_TYPES::PTR64_BIND:         delete p64_bind_;          break;
    case BIND_TYPES::UNKNOWN: {}
  }
  btypes_ = BIND_TYPES::UNKNOWN;
}

}
}
