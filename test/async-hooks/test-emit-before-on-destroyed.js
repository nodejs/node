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
  emitInit, emitBefore, emitAfter, emitDestroy
} = internal_async_hooks;

const initHooks = require('./init-hooks');

if (process.argv[2] === 'child') {
  const hooks = initHooks();
  hooks.enable();

  // Once 'destroy' has been emitted, we can no longer emit 'before'

  // Emitting 'before', 'after' and then 'destroy'
  {
    const asyncId = newAsyncId();
    const triggerId = getDefaultTriggerAsyncId();
    emitInit(asyncId, 'event1', triggerId, {});
    emitBefore(asyncId, triggerId);
    emitAfter(asyncId);
    emitDestroy(asyncId);
  }

  // Emitting 'before' after 'destroy'
  {
    const asyncId = newAsyncId();
    const triggerId = getDefaultTriggerAsyncId();
    emitInit(asyncId, 'event2', triggerId, {});
    emitDestroy(asyncId);

    console.log('heartbeat: still alive');
    emitBefore(asyncId, triggerId);
  }
} else {
  const args = ['--expose-internals']
    .concat(process.argv.slice(1))
    .concat('child');
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
