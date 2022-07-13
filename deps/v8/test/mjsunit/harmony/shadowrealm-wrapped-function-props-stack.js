// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm

(function TestWrappedFunctionNameStackOverflow() {
  const shadowRealm = new ShadowRealm();
  let otherBind = shadowRealm.evaluate('function foo(fn) { return fn.bind(1); }; foo');

  let fn = () => {};
  for(let i = 0; i < 1024 * 50; i++) {
    fn = otherBind(fn.bind(1));
  }
  assertThrows(() => {
    fn.name;
  }, RangeError, 'Maximum call stack size exceeded');
})();
