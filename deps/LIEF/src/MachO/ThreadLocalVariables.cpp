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
#include "LIEF/MachO/ThreadLocalVariables.hpp"
#include "LIEF/MachO/Relocation.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "logging.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF::MachO {
namespace details {
template<class T>
struct Thunk {
  using ptr_t = T;
  ptr_t func;
  ptr_t key;
  ptr_t offset;
};

using Thunk32 = Thunk<uint32_t>;
using Thunk64 = Thunk<uint64_t>;

static_assert(sizeof(Thunk64) == 24);
static_assert(sizeof(Thunk32) == 12);

template<class T>
optional<ThreadLocalVariables::Thunk> get_impl(size_t idx,
                                               span<const uint8_t> buffer) {
  if (idx >= buffer.size() / sizeof(T)) {
    return nullopt();
  }

  const uint64_t offset = idx * sizeof(T);
  const auto* raw = reinterpret_cast<const T*>(buffer.data() + offset);
  return ThreadLocalVariables::Thunk{raw->func, raw->key, raw->offset};
}

template<class T>
bool set_impl(size_t idx, const ThreadLocalVariables::Thunk& thunk,
              span<uint8_t> buffer) {
  using ptr_t = typename T::ptr_t;

  if (idx >= buffer.size() / sizeof(T)) {
    return false;
  }

  const uint64_t offset = idx * sizeof(T);
  auto* raw = reinterpret_cast<T*>(buffer.data() + offset);
  raw->func = (ptr_t)thunk.func;
  raw->key = (ptr_t)thunk.key;
  raw->offset = (ptr_t)thunk.offset;
  return true;
}

}

inline bool is_32bit(const ThreadLocalVariables& thiz) {
  // NOTE(romain): This check is not perfect but it simplifies
  // the logic to identify which Thunk layout to use.
  assert(thiz.segment() != nullptr);
  LoadCommand::TYPE cmd = thiz.segment()->command();
  assert(cmd == LoadCommand::TYPE::SEGMENT ||
         cmd == LoadCommand::TYPE::SEGMENT_64);

  return cmd == LoadCommand::TYPE::SEGMENT ? true : false;
}


ThreadLocalVariables::ThreadLocalVariables() :
  LIEF::MachO::Section() {
  type(TYPE::THREAD_LOCAL_VARIABLES);
}

optional<ThreadLocalVariables::Thunk> ThreadLocalVariables::get(size_t idx) const {
  return is_32bit(*this) ? details::get_impl<details::Thunk32>(idx, content()) :
                           details::get_impl<details::Thunk64>(idx, content());
}


void ThreadLocalVariables::set(size_t idx, const Thunk& thunk) {
  bool ok = is_32bit(*this) ?
                details::set_impl<details::Thunk32>(idx, thunk, content()) :
                details::set_impl<details::Thunk64>(idx, thunk, content());
  if (!ok) {
    LIEF_ERR("Can't set thunk {} at idx #{}", thunk.to_string(), idx);
  }
}

size_t ThreadLocalVariables::nb_thunks() const {
  auto sizeof_ =
      is_32bit(*this) ? sizeof(details::Thunk32) : sizeof(details::Thunk64);
  return content().size() / sizeof_;
}

std::string ThreadLocalVariables::Thunk::to_string() const {
  return fmt::format("func={:#08x}, key={:#08x}, offset={:#08x}", func, key,
                     offset);
}

}
