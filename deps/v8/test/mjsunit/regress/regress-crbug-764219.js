// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  function f(o) {
    o.x = 42;
  };

  f({});
  f(this);
  f(this);
})();

(function() {
  function f(o) {
    o.y = 153;
  };

  Object.setPrototypeOf(this, new Proxy({}, {}));
  f({});
  f(this);
  f(this);
})();

(function() {
  function f(o) {
    o.z = 153;
  };

  Object.setPrototypeOf(this, new Proxy({get z(){}}, {}));
  f({});
  f(this);
  f(this);
})();
