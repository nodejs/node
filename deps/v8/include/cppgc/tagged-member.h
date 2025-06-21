// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_TAGGED_MEMBER_H_
#define INCLUDE_CPPGC_TAGGED_MEMBER_H_

#include <atomic>
#include <cstddef>
#include <type_traits>

#include "cppgc/internal/api-constants.h"
#include "cppgc/macros.h"
#include "cppgc/member.h"
#include "cppgc/visitor.h"

namespace cppgc::subtle {

// The class allows to store a Member along with a single bit tag. It uses
// distinct tag types, Tag1 and Tag2, to represent the two states of the tag.
// The tag is stored in the least significant bit of the pointer.
//
// Example usage:
//   struct ParentTag {};
//   struct ShadowHostTag {};
//
//   /* Constructs a member with the pointer to parent tag: */
//   TaggedUncompressedMember<Node, ParentTag, ShadowHostTag>
//       m(ParentTag{}, parent);
template <typename Pointee, typename Tag1, typename Tag2>
struct TaggedUncompressedMember final {
  CPPGC_DISALLOW_NEW();
  static constexpr uintptr_t kTagBit = 0b1;
  static_assert(kTagBit < internal::api_constants::kAllocationGranularity,
                "The tag must live in the alignment bits of the pointer.");

 public:
  TaggedUncompressedMember(Tag1, Pointee* ptr) : ptr_(ptr) {}
  TaggedUncompressedMember(Tag2, Pointee* ptr)
      : ptr_(reinterpret_cast<Pointee*>(reinterpret_cast<uintptr_t>(ptr) |
                                        kTagBit)) {}

  template <typename Tag>
  Pointee* GetAs() const {
    auto* raw = ptr_.Get();
    if constexpr (std::same_as<Tag, Tag1>) {
      CPPGC_DCHECK(Is<Tag1>());
      return raw;
    } else {
      static_assert(std::same_as<Tag, Tag2>);
      CPPGC_DCHECK(Is<Tag2>());
      return GetUntagged();
    }
  }

  template <typename Tag>
  Pointee* TryGetAs() const {
    auto* raw = ptr_.Get();
    if constexpr (std::same_as<Tag, Tag1>) {
      return (reinterpret_cast<uintptr_t>(raw) & kTagBit) ? nullptr : raw;
    } else {
      static_assert(std::same_as<Tag, Tag2>);
      return (reinterpret_cast<uintptr_t>(raw) & kTagBit)
                 ? reinterpret_cast<Pointee*>(reinterpret_cast<uintptr_t>(raw) &
                                              ~kTagBit)
                 : nullptr;
    }
  }

  Pointee* GetUntagged() const {
    return reinterpret_cast<Pointee*>(reinterpret_cast<uintptr_t>(ptr_.Get()) &
                                      ~kTagBit);
  }

  template <typename Tag>
  void SetAs(Pointee* pointee) {
    if constexpr (std::same_as<Tag, Tag1>) {
      ptr_ = pointee;
    } else {
      static_assert(std::same_as<Tag, Tag2>);
      ptr_ = reinterpret_cast<Pointee*>(reinterpret_cast<uintptr_t>(pointee) |
                                        kTagBit);
    }
  }

  template <typename Tag>
  bool Is() const {
    const bool tag_set = reinterpret_cast<uintptr_t>(ptr_.Get()) & kTagBit;
    if constexpr (std::same_as<Tag, Tag1>) {
      return !tag_set;
    } else {
      static_assert(std::same_as<Tag, Tag2>);
      return tag_set;
    }
  }

  void Trace(Visitor* v) const {
    // Construct an untagged pointer and pass it to Visitor::Trace(). The plugin
    // would warn that ptr_ is untraced, which is why CPPGC_PLUGIN_IGNORE is
    // used.
    UncompressedMember<Pointee> temp(GetUntagged());
    v->Trace(temp);
  }

 private:
  CPPGC_PLUGIN_IGNORE("See Trace()") UncompressedMember<Pointee> ptr_;
};

}  // namespace cppgc::subtle

#endif  // INCLUDE_CPPGC_TAGGED_MEMBER_H_
