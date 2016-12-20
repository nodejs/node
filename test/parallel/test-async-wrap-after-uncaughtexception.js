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

const simId = setImmediate(() => {
  throw new Error('setImmediate');
})._asyncId;
thrown_ids[simId] = 'setImmediate';

const stId = setTimeout(() => {
  throw new Error('setTimeout');
}, 10)._asyncId;
thrown_ids[stId] = 'setTimeout';

const sinId = setInterval(function() {
  clearInterval(this);
  throw new Error('setInterval');
}, 10)._asyncId;
thrown_ids[sinId] = 'setInterval';

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
