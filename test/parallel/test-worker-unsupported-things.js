'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, parentPort } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  process.env.NODE_CHANNEL_FD = 'foo'; // Make worker think it has IPC.
  const w = new Worker(__filename);
  w.on('message', common.mustCall((message) => {
    assert.strictEqual(message, true);
  }));
} else {
  {
    const before = process.title;
    process.title += ' in worker';
    assert.strictEqual(process.title, before);
  }

  {
    const before = process.debugPort;
    process.debugPort++;
    assert.strictEqual(process.debugPort, before);
  }

  const stubs = ['abort', 'chdir', 'send', 'disconnect'];

  if (!common.isWindows) {
    stubs.push('setuid', 'seteuid', 'setgid',
               'setegid', 'setgroups', 'initgroups');
  }

  stubs.forEach((fn) => {
    assert.strictEqual(process[fn].disabled, true);
    assert.throws(() => {
      process[fn]();
    }, { code: 'ERR_WORKER_UNSUPPORTED_OPERATION' });
  });

  ['channel', 'connected'].forEach((fn) => {
    assert.throws(() => {
      process[fn];
    }, { code: 'ERR_WORKER_UNSUPPORTED_OPERATION' });
  });

  assert.strictEqual('_startProfilerIdleNotifier' in process, false);
  assert.strictEqual('_stopProfilerIdleNotifier' in process, false);
  assert.strictEqual('_debugProcess' in process, false);
  assert.strictEqual('_debugPause' in process, false);
  assert.strictEqual('_debugEnd' in process, false);

  parentPort.postMessage(true);
}
