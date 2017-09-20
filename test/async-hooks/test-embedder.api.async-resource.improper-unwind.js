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

  // In both the below two cases 'before' of event2 is nested inside 'before'
  // of event1.
  // Therefore the 'after' of event2 needs to occur before the
  // 'after' of event 1.
  // The first test of the two below follows that rule,
  // the second one doesnt.

  const event1 = new AsyncResource('event1', async_hooks.executionAsyncId());
  const event2 = new AsyncResource('event2', async_hooks.executionAsyncId());

  // Proper unwind
  event1.emitBefore();
  event2.emitBefore();
  event2.emitAfter();
  event1.emitAfter();

  // Improper unwind
  event1.emitBefore();
  event2.emitBefore();

  console.log('heartbeat: still alive');
  event1.emitAfter();
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
