// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_PRELUDE_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_PRELUDE_H_

#include <string_view>

namespace redline {

// JavaScript prepended to every program when it is executed (never part of
// the mutable program itself). The default prelude defines the mjsunit
// assertion API as non-throwing stubs so that imported mjsunit tests and
// their descendants keep running (and covering code) instead of failing on
// the first assertion.
std::string_view DefaultPrelude();

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_PRELUDE_H_
