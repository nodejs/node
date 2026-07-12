// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function clone(src) {
  return { ...src };
}

function inobjectDoubles() {
  "use strict";
  this.p0 = -6400510997704731;
}

// Check that unboxed double is not treated as tagged
assertEquals({ p0: -6400510997704731 }, clone(new inobjectDoubles()));
