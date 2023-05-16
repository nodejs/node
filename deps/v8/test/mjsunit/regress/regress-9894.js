// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function frozen() {
  const ary = [1.1]
  Object.defineProperty(ary, 0, {get:run_it} );

  // v8::internal::Runtime_ArrayIncludes_Slow.
  ary.includes();

  function run_it(el) {
    ary.length = 0;
    ary[0] = 1.1;
    Object.freeze(ary);
    return 2.2;
  }
})();

(function seal() {
  const ary = [1.1]
  Object.defineProperty(ary, 0, {get:run_it} );

  // v8::internal::Runtime_ArrayIncludes_Slow.
  ary.includes();

  function run_it(el) {
    ary.length = 0;
    ary[0] = 1.1;
    Object.seal(ary);
    return 2.2;
  }
})();

(function preventExtensions() {
  const ary = [1.1]
  Object.defineProperty(ary, 0, {get:run_it} );

  // v8::internal::Runtime_ArrayIncludes_Slow.
  ary.includes();

  function run_it(el) {
    ary.length = 0;
    ary[0] = 1.1;
    Object.preventExtensions(ary);
    return 2.2;
  }
})();
