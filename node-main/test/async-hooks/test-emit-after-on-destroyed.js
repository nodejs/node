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
  emitInit, emitBefore, emitAfter, emitDestroy,
} = internal_async_hooks;

const initHooks = require('./init-hooks');

if (process.argv[2] === 'child') {
  const hooks = initHooks();
  hooks.enable();

  // Once 'destroy' has been emitted, we can no longer emit 'after'

  // Emitting 'before', 'after' and then 'destroy'
  {
    const asyncId = newAsyncId();
    const triggerId = getDefaultTriggerAsyncId();
    emitInit(asyncId, 'event1', triggerId, {});
    emitBefore(asyncId, triggerId);
    emitAfter(asyncId);
    emitDestroy(asyncId);
  }

  // Emitting 'after' after 'destroy'
  {
    const asyncId = newAsyncId();
    const triggerId = getDefaultTriggerAsyncId();
    emitInit(asyncId, 'event2', triggerId, {});
    emitDestroy(asyncId);

    console.log('heartbeat: still alive');
    emitAfter(asyncId);
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

  child.on('close', common.mustCall((code, signal) => {
    if ((common.isAIX ||
         (common.isLinux && process.arch === 'x64')) &&
         signal === 'SIGABRT') {
      // XXX: The child process could be aborted due to unknown reasons. Work around it.
    } else {
      assert.strictEqual(signal, null);
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
