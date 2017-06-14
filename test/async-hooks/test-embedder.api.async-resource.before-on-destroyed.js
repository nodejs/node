'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncResource } = async_hooks;
const { spawn } = require('child_process');
const corruptedMsg = /async hook stack has become corrupted/;
const heartbeatMsg = /heartbeat: still alive/;

const initHooks = require('./init-hooks');

if (process.argv[2] === 'child') {
  const hooks = initHooks();
  hooks.enable();

  // once 'destroy' has been emitted, we can no longer emit 'before'

  // Emitting 'before', 'after' and then 'destroy'
  const event1 = new AsyncResource('event1', async_hooks.executionAsyncId());
  event1.emitBefore();
  event1.emitAfter();
  event1.emitDestroy();

  // Emitting 'before' after 'destroy'
  const event2 = new AsyncResource('event2', async_hooks.executionAsyncId());
  event2.emitDestroy();

  console.log('heartbeat: still alive');
  event2.emitBefore();

} else {
  const args = process.argv.slice(1).concat('child');
  let errData = Buffer.from('');
  let outData = Buffer.from('');

  const child = spawn(process.execPath, args);
  child.stderr.on('data', (d) => { errData = Buffer.concat([ errData, d ]); });
  child.stdout.on('data', (d) => { outData = Buffer.concat([ outData, d ]); });

  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 1);
    assert.ok(heartbeatMsg.test(outData.toString()),
              'did not crash until we reached offending line of code ' +
              `(found ${outData})`);
    assert.ok(corruptedMsg.test(errData.toString()),
              'printed error contains corrupted message ' +
              `(found ${errData})`);
  }));
}
