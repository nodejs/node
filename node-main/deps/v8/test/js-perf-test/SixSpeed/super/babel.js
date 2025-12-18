// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This benchmark is based on the six-speed benchmark build output.
// Copyright 2014 Kevin Decker <https://github.com/kpdecker/six-speed/>

'use strict';

new BenchmarkSuite('Babel', [1000], [
  new Benchmark('Babel', false, false, 0, Babel),
]);

var _get = function get(object, property, receiver) {
  if (object === null) object = Function.prototype;
  var desc = Object.getOwnPropertyDescriptor(object, property);
  if (desc === undefined) {
    var parent = Object.getPrototypeOf(object);
    if (parent === null) {
      return undefined;
    } else {
      return get(parent, property, receiver);
    }
  } else if ('value' in desc) {
    return desc.value;
  } else {
    var getter = desc.get;
    if (getter === undefined) {
      return undefined;
    }
    return getter.call(receiver);
  }
};

var _createClass = function() {
  function defineProperties(target, props) {
    for (var i = 0; i < props.length; i++) {
      var descriptor = props[i];
      descriptor.enumerable = descriptor.enumerable || false;
      descriptor.configurable = true;
      if ('value' in descriptor) descriptor.writable = true;
      Object.defineProperty(target, descriptor.key, descriptor);
    }
  }
  return function(Constructor, protoProps, staticProps) {
    if (protoProps) defineProperties(Constructor.prototype, protoProps);
    if (staticProps) defineProperties(Constructor, staticProps);
    return Constructor;
  };
}();

function _possibleConstructorReturn(self, call) {
  if (!self) {
    throw new ReferenceError(
        'this hasn\'t been initialised - super() hasn\'t been called');
  }
  return call && (typeof call === 'object' || typeof call === 'function') ?
      call :
      self;
}

function _inherits(subClass, superClass) {
  if (typeof superClass !== 'function' && superClass !== null) {
    throw new TypeError(
        'Super expression must either be null or a function, not ' +
        typeof superClass);
  }
  subClass.prototype = Object.create(superClass && superClass.prototype, {
    constructor: {
      value: subClass,
      enumerable: false,
      writable: true,
      configurable: true
    }
  });
  if (superClass)
    Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) :
                            subClass.__proto__ = superClass;
}

function _classCallCheck(instance, Constructor) {
  if (!(instance instanceof Constructor)) {
    throw new TypeError('Cannot call a class as a function');
  }
}

var C = function() {
  function C() {
    _classCallCheck(this, C);

    this.foo = 'bar';
  }

  _createClass(C, [{
                 key: 'bar',
                 value: function bar() {
                   return 41;
                 }
               }]);

  return C;
}();

var D = function(_C) {
  _inherits(D, _C);

  function D() {
    _classCallCheck(this, D);

    var _this = _possibleConstructorReturn(
        this, (D.__proto__ || Object.getPrototypeOf(D)).call(this));

    _this.baz = 'bat';
    return _this;
  }

  _createClass(D, [{
                 key: 'bar',
                 value: function bar() {
                   return _get(
                              D.prototype.__proto__ ||
                                  Object.getPrototypeOf(D.prototype),
                              'bar', this)
                              .call(this) +
                       1;
                 }
               }]);

  return D;
}(C);

function Babel() {
  var d = new D();
  return d.bar();
}
