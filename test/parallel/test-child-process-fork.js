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
const assert = require('assert');
const fork = require('child_process').fork;
const args = ['foo', 'bar'];

const n = fork(common.fixturesDir + '/child-process-spawn-node.js', args);

assert.strictEqual(n.channel, n._channel);
assert.deepStrictEqual(args, ['foo', 'bar']);

n.on('message', function(m) {
  console.log('PARENT got message:', m);
  assert.ok(m.foo);
});

// https://github.com/joyent/node/issues/2355 - JSON.stringify(undefined)
// returns "undefined" but JSON.parse() cannot parse that...
assert.throws(function() { n.send(undefined); }, TypeError);
assert.throws(function() { n.send(); }, TypeError);

n.send({ hello: 'world' });

n.on('exit', common.mustCall(function(c) {
  assert.strictEqual(c, 0);
}));
