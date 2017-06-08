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
const cluster = require('cluster');

assert(cluster.isMaster);

// The cluster.settings object is cloned even though the current implementation
// makes that unnecessary. This is to make the test less fragile if the
// implementation ever changes such that cluster.settings is mutated instead of
// replaced.
function cheapClone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

const configs = [];
const execs = [
  'node-next',
  'node-next-2',
  'node-next-3',
];
const calls = execs.length;

// Capture changes
cluster.on('setup', common.mustCall(() => {
  configs.push(cheapClone(cluster.settings));
}, calls));


process.on('exit', function assertTests() {
  // Tests that "setup" is emitted for every call to setupMaster
  assert.strictEqual(configs.length, execs.length);

  for (let i = 0; i < calls; i++) {
    assert.strictEqual(configs[i].exec, execs[i]);
  }
});

// Make changes to cluster settings
execs.forEach((v, i) => {
  setTimeout(() => {
    cluster.setupMaster({ exec: v });
  }, i * 100);
});

// cluster emits 'setup' asynchronously, so we must stay alive long
// enough for that to happen
setTimeout(() => {}, (execs.length + 1) * 100);
