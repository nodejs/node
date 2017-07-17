'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const { fork, spawn } = require('child_process');
const fixtures = require('../common/fixtures');

const emptyFile = fixtures.path('empty.js');

const n = fork(emptyFile);

const rv = n.send({ hello: 'world' });
assert.strictEqual(rv, true);

const spawnOptions = { stdio: ['pipe', 'pipe', 'pipe', 'ipc'] };
const s = spawn(process.execPath, [emptyFile], spawnOptions);
let handle = null;
s.on('exit', function() {
  handle.close();
});

net.createServer(common.mustNotCall()).listen(0, function() {
  handle = this._handle;
  assert.strictEqual(s.send('one', handle), true);
  assert.strictEqual(s.send('two', handle), true);
  assert.strictEqual(s.send('three'), false);
  assert.strictEqual(s.send('four'), false);
});
