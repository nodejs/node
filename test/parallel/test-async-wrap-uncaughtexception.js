'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const async_hooks = require('async_hooks');
const call_log = [0, 0, 0, 0];  // [before, callback, exception, after];
let call_id = null;
let hooks = null;

// TODO(jasnell): This is using process.once because, for some as yet unknown
//                reason, the 'beforeExit' event may be emitted more than once
//                under some conditions on variaous platforms. Using the once
//                handler here avoids the flakiness but ignores the underlying
//                cause of the flakiness.
var n = 0;
process.once('beforeExit', () => {
  console.log('.', call_log);
  if (++n === 2) {
    console.log('x');
    process.exit(1);
  }
  process.removeAllListeners('uncaughtException');
  hooks.disable();
//  assert.strictEqual(typeof call_id, 'number');
//  assert.deepStrictEqual(call_log, [1, 1, 1, 1]);
});


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
