// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --gc-interval=12 --stack-size=50

__f_0();
function __f_0() {
  try {
    __f_0();
  } catch(e) {
    "b".replace(/(b)/g, function() { return "c"; });
  }
}
