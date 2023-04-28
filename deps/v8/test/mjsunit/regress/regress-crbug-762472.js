// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

function Module() {
  "use asm";
  function f() {}
  return { f:f }
}

function InstantiateNearStackLimit() {
  try {
    var fuse = InstantiateNearStackLimit();
    if (fuse == 0) Module();
    return fuse - 1;
  } catch(e) {
    return init_fuse;
  }
}

var init_fuse = 0;
for (init_fuse = 0; init_fuse < 10; init_fuse++) {
  InstantiateNearStackLimit();
}
