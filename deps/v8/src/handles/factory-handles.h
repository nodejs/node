// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_FACTORY_HANDLES_H_
#define V8_HANDLES_FACTORY_HANDLES_H_

namespace v8 {
namespace internal {

template <typename Impl>
struct FactoryTraits;

template <typename Impl, typename T>
using FactoryHandle = typename FactoryTraits<Impl>::template HandleType<T>;
template <typename Impl, typename T>
using FactoryMaybeHandle =
    typename FactoryTraits<Impl>::template MaybeHandleType<T>;

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_FACTORY_HANDLES_H_
