// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test() {
  let promise = undefined;
  let boom = {
    toString: function() {
      assertSame(undefined, promise);
      promise = WebAssembly.instantiate();
      throw new Error('foo');
    }
  };

  assertThrows(
      () => new WebAssembly.Memory({initial: boom, index: boom}), Error, 'foo');
  assertInstanceof(promise, Promise);
  assertThrowsAsync(promise);
})();
