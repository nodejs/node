// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_TYPE_TRAITS_H_
#define V8_ZONE_ZONE_TYPE_TRAITS_H_

#include "src/common/globals.h"

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

struct ZoneTypeTraits {
  template <typename T>
  using Ptr = FullZonePtr<T>;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_TYPE_TRAITS_H_
