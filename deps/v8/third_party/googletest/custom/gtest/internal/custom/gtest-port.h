// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_GOOGLETEST_CUSTOM_GTEST_INTERNAL_CUSTOM_GTEST_PORT_H_
#define THIRD_PARTY_GOOGLETEST_CUSTOM_GTEST_INTERNAL_CUSTOM_GTEST_PORT_H_

// This temporarily forwards tuple and some tuple functions from std::tr1 to
// std:: to make it possible to migrate from std::tr1.
//
// TODO(crbug.com/829773): Remove this file when the transition is complete.

#include <tuple>

namespace std {

namespace tr1 {

using std::get;
using std::make_tuple;
using std::tuple;

}  // namespace tr1

}  // namespace std

#endif  // THIRD_PARTY_GOOGLETEST_CUSTOM_GTEST_INTERNAL_CUSTOM_GTEST_PORT_H_