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
const fixtures = require('../common/fixtures');

const n = fork(fixtures.path('child-process-spawn-node.js'), args);

assert.strictEqual(n.channel, n._channel);
assert.deepStrictEqual(args, ['foo', 'bar']);

n.on('message', (m) => {
  console.log('PARENT got message:', m);
  assert.ok(m.foo);
});

// https://github.com/joyent/node/issues/2355 - JSON.stringify(undefined)
// returns "undefined" but JSON.parse() cannot parse that...
assert.throws(() => n.send(undefined), {
  name: 'TypeError [ERR_MISSING_ARGS]',
  message: 'The "message" argument must be specified',
  code: 'ERR_MISSING_ARGS'
});
assert.throws(() => n.send(), {
  name: 'TypeError [ERR_MISSING_ARGS]',
  message: 'The "message" argument must be specified',
  code: 'ERR_MISSING_ARGS'
});

n.send({ hello: 'world' });

n.on('exit', common.mustCall((c) => {
  assert.strictEqual(c, 0);
}));
