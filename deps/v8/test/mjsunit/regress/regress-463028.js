// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o = {}
Object.defineProperty(o, "z", {
    set: function() {
      %DeoptimizeFunction(f);
    },
});

function f(o) {
  return 19 + (void(o.z = 12));
}

f(o);
