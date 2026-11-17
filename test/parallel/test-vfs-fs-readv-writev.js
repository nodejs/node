// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');
const vfs = require('node:vfs');

tmpdir.refresh();
const mounted = vfs.create();
mounted.mkdirSync('/files', { recursive: true });
const mountPoint = mounted.mount();

const realFd = fs.openSync(tmpdir.resolve('real.txt'), 'w+');
const virtualFd = fs.openSync(path.join(mountPoint, 'files/virtual.txt'), 'w+');
let remaining = 2;

for (const fd of [realFd, virtualFd]) {
  const chunks = [Buffer.from('hello'), Buffer.from('world')];
  fs.writev(fd, chunks, null, common.mustSucceed((bytesWritten, buffers) => {
    assert.strictEqual(bytesWritten, 10);
    assert.strictEqual(buffers, chunks);

    const received = [Buffer.alloc(5), Buffer.alloc(5)];
    fs.readv(fd, received, 0, common.mustSucceed((bytesRead, readBuffers) => {
      assert.strictEqual(bytesRead, 10);
      assert.strictEqual(readBuffers, received);
      assert.strictEqual(Buffer.concat(received).toString(), 'helloworld');
      fs.closeSync(fd);
      if (--remaining === 0) mounted.unmount();
    }));
  }));
}
