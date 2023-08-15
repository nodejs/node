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
    value: function value() {
      spec ??= require('internal/test_runner/reporter/spec');
      return ReflectConstruct(spec, arguments);
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
