'use strict';
require('../common');
const assert = require('assert');

const vm = require('vm');

const code =
    'Object.defineProperty(this, "f", {\n' +
    '  get: function() { return x; },\n' +
    '  set: function(k) { x = k; },\n' +
    '  configurable: true,\n' +
    '  enumerable: true\n' +
    '});\n' +
    'g = f;\n' +
    'f;\n';

const x = {};
const o = vm.createContext({ console: console, x: x });

const res = vm.runInContext(code, o, 'test');

assert(res);
assert.equal(typeof res, 'object');
assert.equal(res, x);
assert.equal(o.f, res);
assert.deepStrictEqual(Object.keys(o), ['console', 'x', 'g', 'f']);
