// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestModifedPrototypeInObjectLiteral() {
  // The prototype chain should not be used if the definition
  // happens in the object literal.

  Object.defineProperty(Object.prototype, 'c', {
    get: function () {
      return 21;
    },
    set: function () {
    }
  });

  var o = {};
  o.c = 7;
  assertEquals(21, o.c);

  var l = {c: 7};
  assertEquals(7, l.c);

  delete Object.prototype.c;
})();
