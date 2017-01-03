// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
