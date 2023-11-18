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
const common = require('../common');
const {
  hijackStdout,
  hijackStderr,
  restoreStdout,
  restoreStderr,
} = require('../common/hijackstdio');
const assert = require('assert');
const util = require('util');

assert.ok(process.stdout.writable);
assert.ok(process.stderr.writable);

const strings = [];
hijackStdout(function(data) {
  strings.push(data);
});
hijackStderr(common.mustNotCall('stderr.write must not be called'));

const tests = [
  { input: 'foo', output: 'foo' },
  { input: undefined, output: 'undefined' },
  { input: null, output: 'null' },
  { input: false, output: 'false' },
  { input: 42, output: '42' },
  { input: function() {}, output: '[Function: input]' },
  { input: parseInt('not a number', 10), output: 'NaN' },
  { input: { answer: 42 }, output: '{ answer: 42 }' },
  { input: [1, 2, 3], output: '[ 1, 2, 3 ]' },
];

// test util.log()
const re = /[0-9]{1,2} [A-Z][a-z]{2} [0-9]{2}:[0-9]{2}:[0-9]{2} - (.+)$/;
for (const test of tests) {
  util.log(test.input);
  const result = strings.shift().trim();
  const match = re.exec(result);
  assert.ok(match);
  assert.strictEqual(match[1], test.output);
}

assert.strictEqual(process.stdout.writeTimes, tests.length);

restoreStdout();
restoreStderr();
