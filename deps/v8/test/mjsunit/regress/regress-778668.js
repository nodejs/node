// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
  function f( __v_59960) {
    arguments.length = -5;
    Array.prototype.slice.call(arguments);
  }
  f('a')
})();

(function () {
  function f( __v_59960) {
    arguments.length = 2.3;
    print(arguments.length);
    Array.prototype.slice.call(arguments);
  }
  f('a')
})();
