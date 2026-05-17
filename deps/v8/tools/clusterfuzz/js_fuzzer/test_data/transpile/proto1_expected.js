// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: proto1.js
(function () {
  A.prototype.foo = function foo() {};
  A.prototype.bar = function bar() {};
})();
(function () {
  var _proto_1 = A.prototype,
    _proto_2 = B.prototype;
  _proto_1.foo = function foo() {};
  _proto_2.bar = function bar() {};
})();
(function () {
  let _proto_3 = A.prototype;
  _proto_3.foo = function foo() {};
})();
(function () {
  var _proto_4;
  _proto_4.foo = function foo() {};
})();
(function () {
  var _proto_5 = callme().prototype;
  _proto_5.foo = function foo() {};
})();
(function () {
  var _proto_6 = A.prototype.foo;
  _proto_6.foo = function foo() {};
})();
(function () {
  var _proto_7 = A.__proto__;
  _proto_7.foo = function foo() {};
})();
(function () {
  A.prototype = meh;
  A.prototype.foo = function foo() {};
  if (false) {}
})();
(function () {
  let x = function () {
    A.prototype.foo = function foo() {};
    B.prototype.bar = function bar() {};
  };
  let y = function () {
    B.prototype.bar = function bar() {};
    A.prototype.foo = function foo() {};
  };
  let z = function () {
    callme(B.prototype);
    console.log(3 + A.prototype);
  };
})();
(function () {
  var proto_11 = A.prototype;
  proto_11.foo = function foo() {};
})();
(function () {
  var _protego_12 = A.prototype;
  _protego_12.foo = function foo() {};
})();
(function () {
  A.prototype.foo = function foo() {};
  A.prototype = B.prototype;
  A.prototype.bar = function bar() {};
})();
