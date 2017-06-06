// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var let = 0;
async function asyncFn() {
  let
  await 0;
  return 0;
}

asyncFn().then(value => {
  assertEquals(0, value);
}, error => {
  assertUnreachable();
}).catch(error => {
  %AbortJS(String(error));
});

%RunMicrotasks();
