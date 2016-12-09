// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-tailcalls

try {
  load("mjsunit/es6/tail-call-megatest.js");
} catch(e) {
  load("test/mjsunit/es6/tail-call-megatest.js");
}

run_tests(0);
