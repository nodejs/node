// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony --allow-natives-syntax --stack-size=100

function __f_0() {
  try {
    return __f_0();
  } catch(e) {
    return import('no-such-file');
  }
}

var done = false;
var error;
var promise = __f_0();
promise.then(assertUnreachable,
             err => { done = true; error = err });
%PerformMicrotaskCheckpoint();
assertTrue(error.startsWith('d8: Error reading'));
assertTrue(done);
