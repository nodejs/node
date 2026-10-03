// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function explore() {}

function Foo() {
  if (!new.target) { throw 'must be called with new'; }
  this.h = 827163141;
  this.b = 827163141;
}

for (let i = 0; i < 5; i++) {
  const o = new Foo();
  for (let [a, b] = (() => {
    return [1073741824, 7263];
  })();
       b--, b;
      ) {
      }
  explore(o);
}
