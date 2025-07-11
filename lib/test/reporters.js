'use strict';

const {
  ObjectDefineProperties,
  ReflectConstruct,
} = primordials;

let dot;
let junit;
let specFn;
let tap;
let lcovFn;

ObjectDefineProperties(module.exports, {
  __proto__: null,
  dot: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() {
      dot ??= require('internal/test_runner/reporter/dot');
      return dot;
    },
  },
  junit: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() {
      junit ??= require('internal/test_runner/reporter/junit');
      return junit;
    },
  },
  spec: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: function spec() {
      specFn ??= require('internal/test_runner/reporter/spec');
      return ReflectConstruct(specFn, arguments);
    },
  },
  tap: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() {
      tap ??= require('internal/test_runner/reporter/tap');
      return tap;
    },
  },
  lcov: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: function lcov() {
      lcovFn ??= require('internal/test_runner/reporter/lcov');
      return ReflectConstruct(lcovFn, arguments);
    },
  },
});
