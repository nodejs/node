'use strict';

const { ObjectDefineProperties, ReflectConstruct } = primordials;

let dot;
let spec;
let tap;

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
  spec: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: spec ?? function SpecReporter() { 
      return ReflectConstruct(require('internal/test_runner/reporter/spec'), arguments); 
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
});
