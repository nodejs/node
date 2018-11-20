// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-opt

load("test/mjsunit/mjsunit.js");

let resolve_handler;
let promise = new Promise(function(resolve, reject) {
  resolve_handler = resolve;
});

assertPromiseResult(promise);

resolve_handler({f: 3463});
