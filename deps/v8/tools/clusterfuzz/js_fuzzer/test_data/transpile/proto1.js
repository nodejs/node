// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests: usage scenarios for inlining proto declarations.

// Accepted: this should inline correctly.
(function() {
  var _proto_0 = A.prototype;
  _proto_0.foo = function foo() {};
  _proto_0.bar = function bar() {};
})();

// Ignored: declaration list.
(function() {
  var _proto_1 = A.prototype, _proto_2 = B.prototype;
  _proto_1.foo = function foo() {};
  _proto_2.bar = function bar() {};
})();

// Ignored: no var declaration.
(function() {
  let _proto_3 = A.prototype;
  _proto_3.foo = function foo() {};
})();

// Ignored: missing initializer.
(function() {
  var _proto_4;
  _proto_4.foo = function foo() {};
})();

// Ignored: the initializer doesn't have a member expression on an identifier.
(function() {
  var _proto_5 = callme().prototype;
  _proto_5.foo = function foo() {};
})();

// Ignored: the initializer doesn't have a member expression with a single
// property.
(function() {
  var _proto_6 = A.prototype.foo;
  _proto_6.foo = function foo() {};
})();

// Ignored: the initializer's property isn't "prototype".
(function() {
  var _proto_7 = A.__proto__;
  _proto_7.foo = function foo() {};
})();

// This is not great, but accepted (we don't expect to ever see such code).
// We can assign to _proto_8 in sloppy mode. The var declaration can be
// anywhere in the code.
(function() {
  _proto_8 = meh;
  _proto_8.foo = function foo() {};

  if (false) var _proto_8 = A.prototype;
})();

// Accepted: all uses of the expressions should be inlined.
(function() {
  var _proto_9 = A.prototype;
  var _proto_10 = B.prototype;
  let x = function () {
    _proto_9.foo = function foo() {};
    _proto_10.bar = function bar() {};
  };
  let y = function () {
    _proto_10.bar = function bar() {};
    _proto_9.foo = function foo() {};
  };
  let z = function () {
    callme(_proto_10);
    console.log(3 + _proto_9);
  };
})();

// Ignored: missing underscore in identifier pattern.
(function() {
  var proto_11 = A.prototype;
  proto_11.foo = function foo() {};
})();

// Ignored: wrong name in identifier pattern
(function() {
  var _protego_12 = A.prototype;
  _protego_12.foo = function foo() {};
})();

// Accepted though not great: we inline also a reassignment
// (we don't expect to see such code often).
(function() {
  var _proto_13 = A.prototype;
  _proto_0.foo = function foo() {};
  _proto_13 = B.prototype;
  _proto_13.bar = function bar() {};
})();
