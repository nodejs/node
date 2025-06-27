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

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const L = require('internal/linkedlist');

const list = { name: 'list' };
const A = { name: 'A' };
const B = { name: 'B' };
const C = { name: 'C' };
const D = { name: 'D' };


L.init(list);
L.init(A);
L.init(B);
L.init(C);
L.init(D);

assert.ok(L.isEmpty(list));
assert.strictEqual(L.peek(list), null);

L.append(list, A);
// list -> A
assert.strictEqual(L.peek(list), A);

L.append(list, B);
// list -> A -> B
assert.strictEqual(L.peek(list), A);

L.append(list, C);
// list -> A -> B -> C
assert.strictEqual(L.peek(list), A);

L.append(list, D);
// list -> A -> B -> C -> D
assert.strictEqual(L.peek(list), A);

L.remove(A);
L.remove(B);
// B is already removed, so removing it again shouldn't hurt.
L.remove(B);
// list -> C -> D
assert.strictEqual(L.peek(list), C);

// Put B back on the list
L.append(list, B);
// list -> C -> D -> B
assert.strictEqual(L.peek(list), C);

L.remove(C);
// list -> D -> B
assert.strictEqual(L.peek(list), D);

L.remove(B);
// list -> D
assert.strictEqual(L.peek(list), D);

L.remove(D);
// list
assert.strictEqual(L.peek(list), null);


assert.ok(L.isEmpty(list));


L.append(list, D);
// list -> D
assert.strictEqual(L.peek(list), D);

L.append(list, C);
L.append(list, B);
L.append(list, A);
// list -> D -> C -> B -> A

// Append should REMOVE C from the list and append it to the end.
L.append(list, C);
// list -> D -> B -> A -> C

assert.strictEqual(L.peek(list), D);
assert.strictEqual(L.peek(D), B);
assert.strictEqual(L.peek(B), A);
assert.strictEqual(L.peek(A), C);
assert.strictEqual(L.peek(C), list);
