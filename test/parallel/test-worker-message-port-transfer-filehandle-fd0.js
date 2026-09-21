'use strict';

// A FileHandle whose transfer is discarded has to close its file descriptor.
// Descriptor 0 is an ordinary descriptor once stdin has been closed, so the
// scenario runs in a child process that can afford to lose its stdin.

const common = require('../common');

if (common.isWindows)
  common.skip('descriptor numbering after closing stdin is POSIX-specific');

const assert = require('assert');
const fs = require('fs');

if (process.argv[2] === 'child') {
  const vm = require('vm');
  const { MessageChannel, moveMessagePortToContext } =
    require('worker_threads');

  (async function() {
    fs.closeSync(0);
    const fh = await fs.promises.open(__filename);
    assert.strictEqual(fh.fd, 0);

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

    assert.throws(() => fs.fstatSync(0), { code: 'EBADF' });
    port1.close();
  })().then(common.mustCall());
  return;
}

const { spawnSync } = require('child_process');
const result = spawnSync(process.execPath, [ __filename, 'child' ],
                         { stdio: [ 'ignore', 'inherit', 'inherit' ] });
assert.strictEqual(result.status, 0);
