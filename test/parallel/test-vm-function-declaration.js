'use strict';
require('../common');
const assert = require('assert');

const vm = require('vm');
const o = vm.createContext({ console: console });

// This triggers the setter callback in node_contextify.cc
let code = 'var a = function() {};\n';

// but this does not, since function decls are defineProperties,
// not simple sets.
code += 'function b(){}\n';

// Grab the global b function as the completion value, to ensure that
// we are getting the global function, and not some other thing
code += '(function(){return this})().b;\n';

const res = vm.runInContext(code, o, 'test');

assert.strictEqual(typeof res, 'function', 'result should be function');
assert.strictEqual(res.name, 'b', 'res should be named b');
assert.strictEqual(typeof o.a, 'function', 'a should be function');
assert.strictEqual(typeof o.b, 'function', 'b should be function');
assert.strictEqual(res, o.b, 'result should be global b function');

console.log('ok');
