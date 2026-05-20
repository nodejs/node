'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const async_hooks = require('async_hooks');
const call_log = [0, 0, 0, 0];  // [before, callback, exception, after];
let call_id = null;
let hooks = null;


process.on('beforeExit', common.mustCall(() => {
  process.removeAllListeners('uncaughtException');
  hooks.disable();
  assert.strictEqual(typeof call_id, 'number');
  assert.deepStrictEqual(call_log, [1, 1, 1, 1]);
}));


hooks = async_hooks.createHook({
  init(id, type) {
    if (type === 'RANDOMBYTESREQUEST')
      call_id = id;
  },
  before(id) {
    if (id === call_id) call_log[0]++;
  },
  after(id) {
    if (id === call_id) call_log[3]++;
  },
}).enable();


process.on('uncaughtException', common.mustCall(() => {
  assert.strictEqual(call_id, async_hooks.executionAsyncId());
  call_log[2]++;
}));


require('crypto').randomBytes(1, common.mustCall(() => {
  assert.strictEqual(call_id, async_hooks.executionAsyncId());
  call_log[1]++;
  throw new Error();
}));
