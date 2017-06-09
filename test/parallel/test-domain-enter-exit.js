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
// Make sure the domain stack is a stack

require('../common');
const assert = require('assert');
const domain = require('domain');

function names(array) {
  return array.map(function(d) {
    return d.name;
  }).join(', ');
}

const a = domain.create();
a.name = 'a';
const b = domain.create();
b.name = 'b';
const c = domain.create();
c.name = 'c';

a.enter(); // push
assert.deepStrictEqual(domain._stack, [a],
                       'a not pushed: ' + names(domain._stack));

b.enter(); // push
assert.deepStrictEqual(domain._stack, [a, b],
                       'b not pushed: ' + names(domain._stack));

c.enter(); // push
assert.deepStrictEqual(domain._stack, [a, b, c],
                       'c not pushed: ' + names(domain._stack));

b.exit(); // pop
assert.deepStrictEqual(domain._stack, [a],
                       'b and c not popped: ' + names(domain._stack));

b.enter(); // push
assert.deepStrictEqual(domain._stack, [a, b],
                       'b not pushed: ' + names(domain._stack));
