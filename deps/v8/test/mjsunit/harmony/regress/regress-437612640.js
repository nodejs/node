// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let a = {
  method() {
    return 42;
  }
};
{
  using b = { [Symbol.dispose]: a.method.bind(a) }
}

(async function() {
  await using b = {[Symbol.asyncDispose]: a.method.bind(a)};
})();
