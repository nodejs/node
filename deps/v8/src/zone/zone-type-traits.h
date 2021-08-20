// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_TYPE_TRAITS_H_
#define V8_ZONE_ZONE_TYPE_TRAITS_H_

#include "src/common/globals.h"

#ifdef V8_COMPRESS_ZONES
#include "src/zone/compressed-zone-ptr.h"
#endif

namespace v8 {
namespace internal {

template <typename T>
class ZoneList;

// ZonePtrList is a ZoneList of pointers to ZoneObjects allocated in the same
// zone as the list object.
template <typename T>
using ZonePtrList = ZoneList<T*>;

template <typename T>
using FullZonePtr = T*;

template <typename T>
class CompressedZonePtr;

//
// ZoneTypeTraits provides type aliases for compressed or full pointer
// dependent types based on a static flag. It helps organizing fine-grained
// control over which parts of the code base should use compressed zone
// pointers.
// For example:
//   using ZoneNodePtr = typename ZoneTypeTraits<kCompressGraphZone>::Ptr<Node>;
//
// or
//   template <typename T>
//   using AstZonePtr = typename ZoneTypeTraits<kCompressAstZone>::Ptr<T>;
//
template <bool kEnableCompression>
struct ZoneTypeTraits;

template <>
struct ZoneTypeTraits<false> {
  template <typename T>
  using Ptr = FullZonePtr<T>;
};

template <>
struct ZoneTypeTraits<true> {
  template <typename T>
  using Ptr = CompressedZonePtr<T>;
};

// This requirement is necessary for being able to use memcopy in containers
// of zone pointers.
// TODO(ishell): Re-enable once compressed pointers are supported in containers.
// static_assert(
//     std::is_trivially_copyable<
//         ZoneTypeTraits<COMPRESS_ZONES_BOOL>::Ptr<int>>::value,
//     "ZoneTypeTraits<COMPRESS_ZONES_BOOL>::Ptr<T> must be trivially
//     copyable");

//
// is_compressed_pointer<T> predicate can be used for checking if T is a
// compressed pointer.
//
template <typename>
struct is_compressed_pointer : std::false_type {};

template <typename T>
struct is_compressed_pointer<CompressedZonePtr<T>> : std::true_type {};

template <typename T>
struct is_compressed_pointer<const CompressedZonePtr<T>> : std::true_type {};

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_TYPE_TRAITS_H_
