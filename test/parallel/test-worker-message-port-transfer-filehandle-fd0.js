'use strict';

// A FileHandle whose transfer is discarded has to close its file descriptor,
// descriptor 0 included. The child gets this file as stdin, so descriptor 0
// refers to it from the start and no other thread can take it before the
// child wraps it in a FileHandle. The child can afford to lose its stdin.

const common = require('../common');

if (common.isWindows)
  common.skip('libuv does not close descriptors 0 to 2 on Windows');

const assert = require('assert');
const fs = require('fs');

if (process.argv[2] === 'child') {
  const vm = require('vm');
  const { MessageChannel, moveMessagePortToContext } =
    require('worker_threads');
  const { internalBinding } = require('internal/test/binding');
  const { FileHandle } = require('internal/fs/promises');
  const { FileHandle: NativeFileHandle } = internalBinding('fs');

  (async function() {
    const { dev, ino } = fs.fstatSync(0, { bigint: true });
    const fh = new FileHandle(new NativeFileHandle(0));

    const { port1, port2 } = new MessageChannel();
    // A port living in another context cannot receive the handle, so the
    // message is discarded on delivery and takes the descriptor with it.
    const moved = moveMessagePortToContext(port2, vm.createContext());
    const discarded = new Promise((resolve) => {
      moved.onmessageerror = resolve;
    });
    moved.start();

    port1.postMessage(fh, [ fh ]);
    await discarded;

    // Another thread may already have reused descriptor 0, but not for this
    // file.
    let stats;
    try {
      stats = fs.fstatSync(0, { bigint: true });
    } catch (err) {
      assert.strictEqual(err.code, 'EBADF');
    }
    if (stats)
      assert.notDeepStrictEqual([stats.dev, stats.ino], [dev, ino]);
    port1.close();
  })().then(common.mustCall());
  return;
}

const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const fd = fs.openSync(__filename);
spawnSyncAndExitWithoutError(process.execPath,
                             [ '--expose-internals', __filename, 'child' ],
                             { stdio: [ fd, 'inherit', 'inherit' ] });
fs.closeSync(fd);
