// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo(arg) {
  var ret = { x: arg };
  ret.__defineSetter__("y", function() { });
  return ret;
}

// v1 creates a map with a Smi field, v2 deprecates v1's map.
let v1 = foo(10);
let v2 = foo(10.5);

// Trigger a PrepareForDataProperty on v1, which also triggers an update to
// dictionary due to the different accessors on v1 and v2's y property.
v1.x = 20.5;
