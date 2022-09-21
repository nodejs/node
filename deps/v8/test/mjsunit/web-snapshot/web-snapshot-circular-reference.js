// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestCircularObjectReference() {
  function createObjects() {
    globalThis.foo = {
      bar: {}
    };
    globalThis.foo.bar.circular = globalThis.foo;
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertSame(foo, foo.bar.circular);
})();
