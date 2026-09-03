// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_CSE_H_
#define V8_MAGLEV_MAGLEV_CSE_H_

#include <array>
#include <tuple>
#include <utility>

#include "src/base/hashing.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

namespace cse {

inline size_t fast_hash_combine(size_t seed, size_t h) {
  // Implementation from boost. Good enough for GVN.
  return h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T>
size_t gvn_hash_value(const T& in) {
  return base::hash_value(in);
}

inline size_t gvn_hash_value(const compiler::MapRef& map) {
  return map.hash_value();
}

inline size_t gvn_hash_value(const interpreter::Register& reg) {
  return base::hash_value(reg.index());
}

inline size_t gvn_hash_value(const Representation& rep) {
  return base::hash_value(rep.kind());
}

inline size_t gvn_hash_value(const ExternalReference& ref) {
  return base::hash_value(ref.address());
}

inline size_t gvn_hash_value(const PolymorphicAccessInfo& access_info) {
  return access_info.hash_value();
}

inline size_t gvn_hash_value(const PropertyKey& key) {
  return base::hash_value(key.data());
}

template <typename T>
size_t gvn_hash_value(const v8::internal::ZoneCompactSet<T>& vector) {
  size_t hash = base::hash_value(vector.size());
  for (auto e : vector) {
    hash = fast_hash_combine(hash, gvn_hash_value(e));
  }
  return hash;
}

template <typename T>
size_t gvn_hash_value(const v8::internal::ZoneVector<T>& vector) {
  size_t hash = base::hash_value(vector.size());
  for (auto e : vector) {
    hash = fast_hash_combine(hash, gvn_hash_value(e));
  }
  return hash;
}

template <typename... Args>
size_t fast_hash_combine(size_t seed, Args&&... args) {
  size_t hash = seed;
  ([&] { hash = cse::fast_hash_combine(hash, cse::gvn_hash_value(args)); }(),
   ...);
  return hash;
}

template <size_t kInputCount, typename... Args>
size_t fast_hash_combine(size_t seed,
                         std::array<ValueNode*, kInputCount>& inputs) {
  size_t hash = seed;
  for (const auto& inp : inputs) {
    hash = cse::fast_hash_combine(hash, base::hash_value(inp));
  }
  return hash;
}

// Canonicalizes the input order of a commutative node so that `a op b` and
// `b op a` hash and compare equal. A no-op for non-commutative nodes.
template <typename NodeT>
void CanonicalizeCommutative(
    std::array<ValueNode*, NodeT::kInputCount>& inputs) {
  if constexpr (IsCommutativeNode(Node::opcode_of<NodeT>)) {
    static_assert(NodeT::kInputCount == 2);
    if ((IsConstantNode(inputs[0]->opcode()) || inputs[0] > inputs[1]) &&
        !IsConstantNode(inputs[1]->opcode())) {
      std::swap(inputs[0], inputs[1]);
    }
  }
}

// Hashes an already-constructed node the same way
// MaglevReducer::AddNewNodeOrGetEquivalent hashes the same node at emission
// time. `inputs` must already be canonicalized (see CanonicalizeCommutative).
template <typename NodeT>
uint32_t HashNode(const NodeT* node,
                  std::array<ValueNode*, NodeT::kInputCount>& inputs) {
  return static_cast<uint32_t>(fast_hash_combine(
      std::apply(
          [](const auto&... opts) {
            return fast_hash_combine(base::hash_value(Node::opcode_of<NodeT>),
                                     opts...);
          },
          node->options()),
      inputs));
}

}  // namespace cse

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_CSE_H_
