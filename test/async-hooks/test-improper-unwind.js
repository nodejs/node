// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const internal_async_hooks = require('internal/async_hooks');
const { spawn } = require('child_process');
const corruptedMsg = /async hook stack has become corrupted/;
const heartbeatMsg = /heartbeat: still alive/;

const {
  newAsyncId, getDefaultTriggerAsyncId,
  emitInit, emitBefore, emitAfter,
} = internal_async_hooks;

const initHooks = require('./init-hooks');

if (process.argv[2] === 'child') {
  const hooks = initHooks();
  hooks.enable();

  // In both the below two cases 'before' of event2 is nested inside 'before'
  // of event1.
  // Therefore the 'after' of event2 needs to occur before the
  // 'after' of event 1.
  // The first test of the two below follows that rule,
  // the second one doesn't.

  const eventOneId = newAsyncId();
  const eventTwoId = newAsyncId();
  const triggerId = getDefaultTriggerAsyncId();
  emitInit(eventOneId, 'event1', triggerId, {});
  emitInit(eventTwoId, 'event2', triggerId, {});

  // Proper unwind
  emitBefore(eventOneId, triggerId);
  emitBefore(eventTwoId, triggerId);
  emitAfter(eventTwoId);
  emitAfter(eventOneId);

  // Improper unwind
  emitBefore(eventOneId, triggerId);
  emitBefore(eventTwoId, triggerId);

  console.log('heartbeat: still alive');
  emitAfter(eventOneId);
} else {
  const args = ['--expose-internals']
    .concat(process.argv.slice(1))
    .concat('child');
  let errData = Buffer.from('');
  let outData = Buffer.from('');

  const child = spawn(process.execPath, args);
  child.stderr.on('data', (d) => { errData = Buffer.concat([ errData, d ]); });
  child.stdout.on('data', (d) => { outData = Buffer.concat([ outData, d ]); });

  child.on('close', common.mustCall((code, signal) => {
    if (signal) {
      console.log(`Child closed with signal: ${signal}`);
    } else {
      assert.strictEqual(code, 1);
    }
    assert.match(outData.toString(), heartbeatMsg,
                 'did not crash until we reached offending line of code ' +
                 `(found ${outData})`);
    assert.match(errData.toString(), corruptedMsg,
                 'printed error contains corrupted message ' +
                 `(found ${errData})`);
  }));
}
