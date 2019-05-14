// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap --expose-gc

let paramName = '';
for (let i=0; i < 2**10; i++) {
  paramName += 'a';
}

let params = '';
for (let i = 0; i < 2**10; i++) {
  params += paramName + i + ',';
}

let fn = eval(`(
  class A {
    constructor (${params}) {
      function lazy() {
        return function lazier() { return ${paramName+1} }
      };
      return lazy;
    }
})`);

gc()
