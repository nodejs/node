'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const thrown_ids = {};

process.on('exit', () => {
  process.removeAllListeners('uncaughtException');
  assert.strictEqual(Object.keys(thrown_ids).length, 0, 'ids remain');
});

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, thrown_ids[async_hooks.currentId()]);
}, 5));

async_hooks.createHook({
  after(id) {
    delete thrown_ids[id];
  },
}).enable();

const si = setImmediate(() => {
  throw new Error('setImmediate');
});
let async_id_symbol = null;
for (const i of Object.getOwnPropertySymbols(si)) {
  if (i.toString() === 'Symbol(asyncId)') {
    async_id_symbol = i;
    break;
  }
}
assert.ok(async_id_symbol !== null);
thrown_ids[si[async_id_symbol]] = 'setImmediate';

const st = setTimeout(() => {
  throw new Error('setTimeout');
}, 10);
thrown_ids[st[async_id_symbol]] = 'setTimeout';

const sin = setInterval(function() {
  clearInterval(this);
  throw new Error('setInterval');
}, 10);
thrown_ids[sin[async_id_symbol]] = 'setInterval';

const rbId = require('crypto').randomBytes(1, () => {
  throw new Error('RANDOMBYTESREQUEST');
}).getAsyncId();
thrown_ids[rbId] = 'RANDOMBYTESREQUEST';

const tcpId = require('net').createServer(function(c) {
  c.end();
  setImmediate(() => this.close());
  throw new Error('TCPWRAP');
}).listen(common.PORT)._handle.getAsyncId();
thrown_ids[tcpId] = 'TCPWRAP';
require('net').connect(common.PORT, () => {}).resume();
