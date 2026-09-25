// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: v8/test/mjsunit/mjsunit.js
const some_arrow = () => false;

// Original: v8/test/mjsunit/test1.js
// Transpiled ES6->ES5.
function _defineProperties(e, r) {
  for (var t = 0; t < r.length; t++) {
    var o = r[t];
    o.enumerable = o.enumerable || !1, o.configurable = !0, "value" in o && (o.writable = !0), Object.defineProperty(e, _toPropertyKey(o.key), o);
  }
}
function _createClass(e, r, t) {
  return r && _defineProperties(e.prototype, r), t && _defineProperties(e, t), Object.defineProperty(e, "prototype", {
    writable: !1
  }), e;
}
function _toPropertyKey(t) {
  var i = _toPrimitive(t, "string");
  return "symbol" == typeof i ? i : i + "";
}
function _toPrimitive(t, r) {
  if ("object" != typeof t || !t) return t;
  var e = t[Symbol.toPrimitive];
  if (void 0 !== e) {
    var i = e.call(t, r || "default");
    if ("object" != typeof i) return i;
    throw new TypeError("@@toPrimitive must return a primitive value.");
  }
  return ("string" === r ? String : Number)(t);
}
function _inheritsLoose(t, o) {
  t.prototype = Object.create(o.prototype), t.prototype.constructor = t, _setPrototypeOf(t, o);
}
function _setPrototypeOf(t, e) {
  return _setPrototypeOf = Object.setPrototypeOf ? Object.setPrototypeOf.bind() : function (t, e) {
    return t.__proto__ = e, t;
  }, _setPrototypeOf(t, e);
}
function _classPrivateFieldLooseBase(e, t) {
  if (!{}.hasOwnProperty.call(e, t)) throw new TypeError("attempted to use private field on non-instance");
  return e;
}
var id = 0;
function _classPrivateFieldLooseKey(e) {
  return "__private_" + id++ + "_" + e;
}
let A = function () {
  function A() {
    this.a = 1;
    this.b = 2;
  }
  A.prototype.foo = function foo() {
    throw 42;
  };
  A.prototype.bar = function bar() {
    return 42;
  };
  return A;
}();
var _privateryan = _classPrivateFieldLooseKey("privateryan");
var _ryan = _classPrivateFieldLooseKey("ryan");
let B = function (_A2) {
  function B(...args) {
    var _this;
    _this = _A2.call(this, ...args) || this;
    Object.defineProperty(_this, _ryan, {
      get: _get_ryan,
      set: void 0
    });
    Object.defineProperty(_this, _privateryan, {
      writable: true,
      value: "huh"
    });
    _this.c = 3;
    return _this;
  }
  _inheritsLoose(B, _A2);
  B.prototype.baz = function baz() {
    %OptimizeFunctionOnNextCall(dontchokeonthis);
    return 42;
  };
  B.prototype[Symbol.dispose] = function () {
    return 0;
  };
  B.meh = function meh() {};
  return _createClass(B, [{
    key: "0",
    get: function () {
      return 1;
    }
  }]);
}(A);
function _get_ryan() {
  return _classPrivateFieldLooseBase(this, _privateryan)[_privateryan];
}
console.log(42);
const a = new A();
const b = new B();
