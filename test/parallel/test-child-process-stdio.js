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
const spawnSync = require('child_process').spawnSync;

let options = { stdio: ['pipe'] };
let child = common.spawnPwd(options);

assert.notStrictEqual(child.stdout, null);
assert.notStrictEqual(child.stderr, null);

options = { stdio: 'ignore' };
child = common.spawnPwd(options);

assert.strictEqual(child.stdout, null);
assert.strictEqual(child.stderr, null);

options = { stdio: 'ignore' };
child = spawnSync('cat', [], options);
assert.deepStrictEqual(options, { stdio: 'ignore' });

assert.throws(() => {
  common.spawnPwd({ stdio: ['pipe', 'pipe', 'pipe', 'ipc', 'ipc'] });
}, common.expectsError({ code: 'ERR_IPC_ONE_PIPE', type: Error }));
