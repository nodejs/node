// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  return globalThis;
}
class C extends f {
  ["vvv"] = 5;
  vv = 5;
}
new C();
for (i = 0; i < 20; i++) {
  new C();
}
