// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_GMOCK_CUSTOM_GMOCK_INTERNAL_CUSTOM_GMOCK_PORT_H_
#define TESTING_GMOCK_CUSTOM_GMOCK_INTERNAL_CUSTOM_GMOCK_PORT_H_

#include <type_traits>

namespace std {

// Provide alternative implementation of std::is_default_constructible for
// old, pre-4.7 of libstdc++, where is_default_constructible is missing.
// <20120322 below implies pre-4.7.0. In addition we blacklist several version
// that released after 4.7.0 from pre-4.7.0 branch. 20120702 implies 4.5.4, and
// 20121127 implies 4.6.4.
#if defined(__GLIBCXX__) &&                               \
    (__GLIBCXX__ < 20120322 || __GLIBCXX__ == 20120702 || \
     __GLIBCXX__ == 20121127)
template <typename T>
using is_default_constructible = std::is_constructible<T>;
#endif
}

#endif  // TESTING_GMOCK_CUSTOM_GMOCK_INTERNAL_CUSTOM_GMOCK_PORT_H_
