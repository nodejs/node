// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function main() {
  function test(v9,v10) {
    const v14 = async (v15,v16,v17) => {
      function v19() {
        JSON.stringify(this);
        v17(test);
      }
      Object.defineProperty(v17,v16,v10);
      new Promise(v19);
    }
    v9.__proto__.__proto__ = new Proxy(test, {get: v14})
  }
  test(isFinite,{});
  undefined_function();
}
assertThrows(main);
