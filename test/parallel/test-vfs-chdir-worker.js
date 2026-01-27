'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { Worker, isMainThread, parentPort, workerData } = require('worker_threads');

if (isMainThread) {
  // Test 1: Verify that VFS setup in main thread doesn't automatically apply to workers
  {
    const vfs = fs.createVirtual({ virtualCwd: true });
    vfs.mkdirSync('/project', { recursive: true });
    vfs.mount('/virtual');

    // Set virtual cwd in main thread
    process.chdir('/virtual/project');
    assert.strictEqual(process.cwd(), '/virtual/project');

    // Worker should not have the hooked process.chdir/cwd
    const worker = new Worker(__filename, {
      workerData: { test: 'main-thread-vfs-not-shared' },
    });

    worker.on('message', common.mustCall((msg) => {
      // Worker's process.cwd() should return the real cwd (not /virtual/project)
      // because VFS hooks are not automatically shared with workers
      assert.notStrictEqual(msg.cwd, '/virtual/project');
    }));

    worker.on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0);
      vfs.unmount();
    }));
  }

  // Test 2: VFS can be set up independently in a worker
  {
    const worker = new Worker(__filename, {
      workerData: { test: 'worker-independent-vfs' },
    });

    worker.on('message', common.mustCall((msg) => {
      assert.strictEqual(msg.success, true);
      assert.strictEqual(msg.cwd, '/worker-virtual/data');
    }));

    worker.on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0);
    }));
  }

  // Test 3: Worker can use VFS passed via workerData (not the hooks, but the VFS module)
  {
    const worker = new Worker(__filename, {
      workerData: { test: 'worker-create-vfs' },
    });

    worker.on('message', common.mustCall((msg) => {
      assert.strictEqual(msg.success, true);
      assert.strictEqual(msg.virtualCwdEnabled, true);
      assert.strictEqual(msg.vfsCwd, '/project/src');
    }));

    worker.on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0);
    }));
  }

} else {
  // Worker thread code
  const { test } = workerData;

  if (test === 'main-thread-vfs-not-shared') {
    // Simply report our cwd
    parentPort.postMessage({ cwd: process.cwd() });
  } else if (test === 'worker-independent-vfs') {
    // Set up VFS independently in worker
    const vfs = fs.createVirtual({ virtualCwd: true });
    vfs.mkdirSync('/data', { recursive: true });
    vfs.mount('/worker-virtual');

    process.chdir('/worker-virtual/data');
    const cwd = process.cwd();

    vfs.unmount();

    parentPort.postMessage({ success: true, cwd });
  } else if (test === 'worker-create-vfs') {
    // Test VFS creation and chdir in worker
    const vfs = fs.createVirtual({ virtualCwd: true });
    vfs.mkdirSync('/project/src', { recursive: true });
    vfs.mount('/');

    vfs.chdir('/project/src');

    parentPort.postMessage({
      success: true,
      virtualCwdEnabled: vfs.virtualCwdEnabled,
      vfsCwd: vfs.cwd(),
    });

    vfs.unmount();
  }
}
