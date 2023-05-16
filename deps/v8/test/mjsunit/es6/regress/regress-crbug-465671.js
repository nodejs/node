// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This used to trigger crash because of an unhandled stack overflow.
function f() {
  var a = [10];
  try {
    f();
  } catch(e) {
    a.map(v => v + 1);
  }
}
f();
