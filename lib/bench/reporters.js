'use strict';

const {
  ObjectDefineProperties,
  ReflectConstruct,
} = primordials;
const { emitExperimentalWarning } = require('internal/util');

let json;
let specFn;

emitExperimentalWarning('Benchmarks');

ObjectDefineProperties(module.exports, {
  __proto__: null,
  json: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() {
      json ??= require('internal/bench_runner/reporter/json');
      return json;
    },
  },
  spec: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: function spec() {
      specFn ??= require('internal/bench_runner/reporter/spec');
      return ReflectConstruct(specFn, arguments);
    },
  },
});
