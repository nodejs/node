// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-explicit-resource-management

(async function testAsyncIteratorPrototypeAsyncDisposeArgCount() {
  async function* generator() {}
  const AsyncIteratorPrototype =
      Object.getPrototypeOf(Object.getPrototypeOf(generator.prototype));

  const iter = Object.create(AsyncIteratorPrototype);
  let returnCalled = false;
  let argumentCount = -1;

  iter.return = async function() {
    argumentCount = arguments.length;
    returnCalled = true;
    return { done: true };
  };

  await iter[Symbol.asyncDispose]();
  assertTrue(returnCalled);
  assertEquals(0, argumentCount);
})();
