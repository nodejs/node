// Flags: --experimental-vfs
'use strict';

// The promise-based fs methods that accept `encoding: 'buffer'` must convert
// the (string) provider result into a Buffer before resolving.

const common = require('../common');
const assert = require('assert');
const fsp = require('fs/promises');
const path = require('path');
const vfs = require('node:vfs');

function mounted() {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello');
  const mountPoint = myVfs.mount();
  return { myVfs, mountPoint };
}

(async () => {
  // readdir
  {
    const { myVfs, mountPoint } = mounted();
    const entries = await fsp.readdir(path.join(mountPoint, 'src'),
                                      { encoding: 'buffer' });
    assert.ok(entries.every(Buffer.isBuffer));
    assert.ok(entries.some((b) => b.toString() === 'hello.txt'));
    myVfs.unmount();
  }

  // realpath
  {
    const { myVfs, mountPoint } = mounted();
    const p = path.join(mountPoint, 'src/hello.txt');
    const rp = await fsp.realpath(p, { encoding: 'buffer' });
    assert.ok(Buffer.isBuffer(rp));
    assert.strictEqual(rp.toString(), p);
    myVfs.unmount();
  }

  // readlink
  {
    const { myVfs, mountPoint } = mounted();
    await fsp.symlink('hello.txt', path.join(mountPoint, 'src/ln.txt'));
    const target = await fsp.readlink(path.join(mountPoint, 'src/ln.txt'),
                                      { encoding: 'buffer' });
    assert.ok(Buffer.isBuffer(target));
    assert.strictEqual(target.toString(), 'hello.txt');
    myVfs.unmount();
  }

  // mkdtemp
  {
    const { myVfs, mountPoint } = mounted();
    const dir = await fsp.mkdtemp(path.join(mountPoint, 'src/td-'),
                                  { encoding: 'buffer' });
    assert.ok(Buffer.isBuffer(dir));
    myVfs.unmount();
  }
})().then(common.mustCall());
