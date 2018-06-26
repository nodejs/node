// Flags: --experimental-worker
'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

if (isMainThread) {
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

  assert.strictEqual('abort' in process, false);
  assert.strictEqual('chdir' in process, false);
  assert.strictEqual('setuid' in process, false);
  assert.strictEqual('seteuid' in process, false);
  assert.strictEqual('setgid' in process, false);
  assert.strictEqual('setegid' in process, false);
  assert.strictEqual('setgroups' in process, false);
  assert.strictEqual('initgroups' in process, false);

  assert.strictEqual('_startProfilerIdleNotifier' in process, false);
  assert.strictEqual('_stopProfilerIdleNotifier' in process, false);
  assert.strictEqual('_debugProcess' in process, false);
  assert.strictEqual('_debugPause' in process, false);
  assert.strictEqual('_debugEnd' in process, false);

  parentPort.postMessage(true);
}
