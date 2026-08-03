// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

function foo() {
  Object.defineProperty({}, undefined, { set: function () {} });
  obj = {};
  Object.defineProperty(obj, undefined, { get: function () {} });
  obj.x = 0;
  obj.y = 0;
  obj[Symbol.toStringTag] = "foo";
}
foo();
assertEquals('[object foo]', obj.toString());
foo();
assertEquals('[object foo]', obj.toString());
