// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let success = false;
function f() {
  success = (f.caller.arguments === null);
}
Promise.resolve().then(f);
%RunMicrotasks();
assertTrue(success);
