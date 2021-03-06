// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var asdf = false;

const f =
  (v1 = (function g() {
    if (asdf) { return; } else { return; }
    (function h() {});
  })()) => 1;
f();
