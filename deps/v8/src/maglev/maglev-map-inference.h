// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_MAP_INFERENCE_H_
#define V8_MAGLEV_MAGLEV_MAP_INFERENCE_H_

#include <iostream>

#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-known-node-aspects.h"

namespace v8 {
namespace internal {
namespace maglev {

// This class is a thin wrapper around fetching and using maps known for
// `object`.
class MapInference {
 public:
  enum Variant {
    // Use possible_maps only if all are fresh, ie no stale unreliable maps
    // exist. In this variant, no map checks are needed since we are fully
    // protected by construction (by previous map checks, and by compilation
    // dependencies).
    kOnlyFresh,
    // Use all forms of possible_maps. Map checks may have to be emitted.
    kAll,
  };

  MapInference(MaglevGraphBuilder* builder, ValueNode* object)
      : MapInference(builder, object,
                     V8_LIKELY(v8_flags.maglev_use_unreliable_maps)
                         ? Variant::kAll
                         : Variant::kOnlyFresh) {}

  MapInference(MaglevGraphBuilder* builder, ValueNode* object, Variant variant)
      : builder_(builder),
        object_(object),
        node_info_(builder_->known_node_aspects().TryGetInfoFor(object)),
        variant_(variant) {}

  bool HaveMaps() const {
    if (!node_info_) return false;
    return node_info_->possible_maps_are_known();
  }

  bool all_maps_are_fresh() const {
    return HaveMaps() && !node_info_->maps_are_stale();
  }

  bool all_maps_are_stable() const {
    return HaveMaps() && !node_info_->any_map_or_node_type_is_unstable();
  }

  ReduceResult InsertMapChecks(Zone* zone) {
    if (variant_ == kOnlyFresh) {
      DCHECK(all_maps_are_fresh());
      return ReduceResult::Done();
    }
    if (!node_info_) return ReduceResult::Done();
    if (!node_info_->maps_are_stale()) return ReduceResult::Done();

    // maps_are_stale implies the presence of unstable maps.
    DCHECK(node_info_->any_map_or_node_type_is_unstable());

    // We've recorded stale unstable maps. Insert map checks.
    const PossibleMaps& maps = node_info_->possible_maps();
    if (!maps.is_empty()) {
      if (V8_UNLIKELY(v8_flags.trace_maglev_graph_building)) {
        std::cout << "  * MapInference emitting map checks for "
                  << PrintNodeLabel(object_) << std::endl;
      }

      // `maps` uses linear storage, but unfortunately we cannot exploit that
      // easily for BuildCheckMaps since it stores ObjectData underneath, which
      // needs to be converted to MapRefs first.
      SmallZoneVector<compiler::MapRef, 8> maps_vector(zone);
      maps_vector.reserve(maps.size());
      for (compiler::MapRef m : maps) {
        maps_vector.push_back(m);
      }

      RETURN_IF_ABORT(
          builder_->BuildCheckMaps(object_, base::VectorOf(maps_vector)));
    }

    // Maps are now fresh.
    node_info_->MarkFresh();

    // We have unstable maps and must re-enable invalidation tracking.
    builder_->known_node_aspects().MarkSideEffectsRequireInvalidation();

    return ReduceResult::Done();
  }

  std::optional<PossibleMaps> TryGetPossibleMaps() const {
    if (!node_info_) return {};
    if (!node_info_->possible_maps_are_known()) return {};
    if (variant_ == kOnlyFresh && node_info_->maps_are_stale()) return {};
    return node_info_->possible_maps();
  }

 private:
  MaglevGraphBuilder* const builder_;
  ValueNode* const object_;
  NodeInfo* const node_info_;
  const Variant variant_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_MAP_INFERENCE_H_
