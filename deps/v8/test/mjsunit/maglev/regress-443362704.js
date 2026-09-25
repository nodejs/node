// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function wrap(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}

function foo() {
  const n = wrap(() => 5);

  try {
    for (let i = 0; i < 28; i++) {}
  } catch (e) {}

  wrap(() => n);
}

for (let i = 0; i < 100; i++) {
  foo();
}
