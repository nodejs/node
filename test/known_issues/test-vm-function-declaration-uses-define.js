'use strict';

// https://github.com/nodejs/node/issues/31808
// function declarations currently call [[Set]] instead of [[DefineOwnProperty]]
// in VM contexts, which violates the ECMA-262 specification:
// https://tc39.es/ecma262/#sec-createglobalfunctionbinding

const common = require('../common');
const vm = require('vm');
const assert = require('assert');

const ctx = vm.createContext();
Object.defineProperty(ctx, 'x', {
  enumerable: true,
  configurable: true,
  get: common.mustNotCall('ctx.x getter must not be called'),
  set: common.mustNotCall('ctx.x setter must not be called'),
});

vm.runInContext('function x() {}', ctx);
assert.strictEqual(typeof ctx.x, 'function');
