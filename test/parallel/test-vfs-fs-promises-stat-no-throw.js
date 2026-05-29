// Flags: --experimental-vfs
'use strict';

// fsp.stat() with throwIfNoEntry: false on a missing path within a mount
// must resolve with undefined instead of rejecting with ENOENT.

const common = require('../common');
const assert = require('assert');
const fsp = require('fs/promises');
const path = require('path');
const vfs = require('node:vfs');

(async () => {
  const mountPoint = path.resolve('/tmp/vfs-stat-no-throw-' + process.pid);
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello');
  myVfs.mount(mountPoint);

  // Missing file -> undefined
  const missing = await fsp.stat(path.join(mountPoint, 'src/nope'),
                                 { throwIfNoEntry: false });
  assert.strictEqual(missing, undefined);

  // Existing file -> normal Stats
  const stats = await fsp.stat(path.join(mountPoint, 'src/hello.txt'),
                               { throwIfNoEntry: false });
  assert.strictEqual(stats.isFile(), true);

  // Default behaviour (no option) still rejects on ENOENT
  await assert.rejects(fsp.stat(path.join(mountPoint, 'src/nope')),
                       { code: 'ENOENT' });

  myVfs.unmount();
})().then(common.mustCall());
