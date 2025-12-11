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

// Trivial test of fs.readFile on an empty file.
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const fn = fixtures.path('empty.txt');

fs.readFile(fn, common.mustCall((err, data) => {
  assert.ok(data);
}));

fs.readFile(fn, 'utf8', common.mustCall((err, data) => {
  assert.strictEqual(data, '');
}));

fs.readFile(fn, { encoding: 'utf8' }, common.mustCall((err, data) => {
  assert.strictEqual(data, '');
}));

assert.ok(fs.readFileSync(fn));
assert.strictEqual(fs.readFileSync(fn, 'utf8'), '');
