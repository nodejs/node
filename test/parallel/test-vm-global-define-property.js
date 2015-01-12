var common = require('../common');
var assert = require('assert');

var vm = require('vm');

var code =
    'Object.defineProperty(this, "f", {\n' +
    '  get: function() { return x; },\n' +
    '  set: function(k) { x = k; },\n' +
    '  configurable: true,\n' +
    '  enumerable: true\n' +
    '});\n' +
    'g = f;\n' +
    'f;\n';

var x = {};
var o = vm.createContext({ console: console, x: x });

var res = vm.runInContext(code, o, 'test');

assert(res);
assert.equal(typeof res, 'object');
assert.equal(res, x);
assert.equal(o.f, res);
assert.deepEqual(Object.keys(o), ['console', 'x', 'g', 'f']);
