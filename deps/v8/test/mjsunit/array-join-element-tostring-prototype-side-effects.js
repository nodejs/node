// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function ArrayPrototypeChanged() {
  const el = {
    toString() {
      Array.prototype[1] = '2';
      return '1';
    }
  };
  const a = [el, ,3];
  assertSame("123", a.join(''));
})();
