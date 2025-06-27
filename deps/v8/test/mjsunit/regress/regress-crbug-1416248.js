// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class foo {
  get [Symbol.toStringTag]() {
    return "foo";
  }
}
let o = new foo();
assertEquals('[object foo]', o.toString());
