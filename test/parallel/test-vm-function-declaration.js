'use strict';
require('../common');
var assert = require('assert');

var vm = require('vm');
var o = vm.createContext({ console: console });

// This triggers the setter callback in node_contextify.cc
var code = 'var a = function() {};\n';

// but this does not, since function decls are defineProperties,
// not simple sets.
code += 'function b(){}\n';

// Grab the global b function as the completion value, to ensure that
// we are getting the global function, and not some other thing
code += '(function(){return this})().b;\n';

var res = vm.runInContext(code, o, 'test');

assert.equal(typeof res, 'function', 'result should be function');
assert.equal(res.name, 'b', 'res should be named b');
assert.equal(typeof o.a, 'function', 'a should be function');
assert.equal(typeof o.b, 'function', 'b should be function');
assert.equal(res, o.b, 'result should be global b function');

console.log('ok');
