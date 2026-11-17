// Flags: --disable-warning=ExperimentalWarning
'use strict';

// A worker does not share the VFS mounts of the thread that starts it, so a
// worker script in a VFS mounted with vfs.mount() cannot be loaded. The worker
// reports that, instead of failing on the real file system (on Windows, as an
// invalid package.json, because paths under the VFS root open the NUL device).

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');
const { Worker } = require('worker_threads');

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/worker.js', "require('worker_threads').parentPort.postMessage('ran');\n");
  const mountPoint = myVfs.mount();
  const filename = path.join(mountPoint, 'worker.js');

  const worker = new Worker(filename);
  worker.on('message', common.mustNotCall());
  worker.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_VFS_NOT_MOUNTED');
    assert.strictEqual(
      err.message,
      `${filename} is in a virtual file system that is not mounted in this thread`);
    myVfs.unmount();
  }));
}

// A real worker script whose path merely mentions "vfs" runs as usual.
{
  tmpdir.refresh();
  const filename = tmpdir.resolve('vfs-worker.js');
  fs.writeFileSync(filename, "require('worker_threads').parentPort.postMessage('ran');\n");

  const worker = new Worker(filename);
  worker.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 'ran');
  }));
  worker.on('error', common.mustNotCall());
}
