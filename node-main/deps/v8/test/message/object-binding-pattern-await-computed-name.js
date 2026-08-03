// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function f() {
  let { [await "a"]: a } = { a: 1 };
  return a;
}
f();
