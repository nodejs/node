// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache=after-execute

function g() {
  function h() {
    function k() { return 0; };
    return k;
  }
  return h();
}

g();
