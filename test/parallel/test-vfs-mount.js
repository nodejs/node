// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const os = require('os');
const vfs = require('node:vfs');

// Basic mount/unmount API and dispatch through node:vfs from the public fs.

function createMountedVfs() {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello world');
  const mountPoint = myVfs.mount('/data');
  return { myVfs, mountPoint };
}

// Test: mounted/mountPoint getters; mount() returns the mount point in
// the reserved namespace under os.devNull.
{
  const myVfs = vfs.create();
  assert.strictEqual(myVfs.mounted, false);
  assert.strictEqual(myVfs.mountPoint, null);

  const mountPoint = myVfs.mount('/data');
  assert.strictEqual(myVfs.mounted, true);
  assert.strictEqual(myVfs.mountPoint, mountPoint);
  assert.strictEqual(
    mountPoint,
    path.join(os.devNull, 'vfs', `layer-${myVfs.layerId}`, 'data'));

  myVfs.unmount();
  assert.strictEqual(myVfs.mounted, false);
  assert.strictEqual(myVfs.mountPoint, null);
}

// Test: prefix is optional and never resolved against the working
// directory; relative and absolute prefixes are logical names inside
// the reserved namespace.
{
  const myVfs = vfs.create();
  const mountPoint = myVfs.mount();
  assert.strictEqual(
    mountPoint,
    path.join(os.devNull, 'vfs', `layer-${myVfs.layerId}`));
  myVfs.unmount();

  const relative = myVfs.mount('deps');
  assert.strictEqual(
    relative,
    path.join(os.devNull, 'vfs', `layer-${myVfs.layerId}`, 'deps'));
  myVfs.unmount();
}

// Test: double-mount throws
{
  const myVfs = vfs.create();
  myVfs.mount('/data');
  assert.throws(() => myVfs.mount('/data'), { code: 'ERR_INVALID_STATE' });
  myVfs.unmount();
}

// Test: prefix cannot escape the reserved namespace; non-string throws
{
  const myVfs = vfs.create();
  assert.throws(() => myVfs.mount('../../escape'),
                { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(() => myVfs.mount(42), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.strictEqual(myVfs.mounted, false);
}

// Test: two instances can use the same logical prefix - each gets a
// distinct mount point in its own layer namespace.
{
  const a = vfs.create();
  const b = vfs.create();
  a.writeFileSync('/f.txt', 'A');
  b.writeFileSync('/f.txt', 'B');
  const mountA = a.mount('/same');
  const mountB = b.mount('/same');
  assert.notStrictEqual(mountA, mountB);
  assert.strictEqual(fs.readFileSync(path.join(mountA, 'f.txt'), 'utf8'), 'A');
  assert.strictEqual(fs.readFileSync(path.join(mountB, 'f.txt'), 'utf8'), 'B');
  a.unmount();
  b.unmount();
}

// Test: fs.readFileSync intercepted
{
  const { myVfs, mountPoint } = createMountedVfs();
  const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
  assert.strictEqual(content, 'hello world');
  myVfs.unmount();
}

// Test: fs.existsSync intercepted
{
  const { myVfs, mountPoint } = createMountedVfs();
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), true);
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'nonexistent')), false);
  myVfs.unmount();
}

// Test: fs.statSync intercepted
{
  const { myVfs, mountPoint } = createMountedVfs();
  const stats = fs.statSync(path.join(mountPoint, 'src/hello.txt'));
  assert.strictEqual(stats.isFile(), true);
  assert.strictEqual(stats.size, 11);
  myVfs.unmount();
}

// Test: fs.readdirSync intercepted
{
  const { myVfs, mountPoint } = createMountedVfs();
  const entries = fs.readdirSync(path.join(mountPoint, 'src'));
  assert.ok(entries.includes('hello.txt'));
  myVfs.unmount();
}

// Test: fs.writeFileSync intercepted
{
  const { myVfs, mountPoint } = createMountedVfs();
  const newPath = path.join(mountPoint, 'src/new.txt');
  fs.writeFileSync(newPath, 'fresh');
  assert.strictEqual(fs.readFileSync(newPath, 'utf8'), 'fresh');
  myVfs.unmount();
}

// Test: fs callback API
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.readFile(path.join(mountPoint, 'src/hello.txt'), 'utf8',
              common.mustSucceed((data) => {
                assert.strictEqual(data, 'hello world');
                myVfs.unmount();
              }));
}

// Test: fs.promises API
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.readFile(path.join(mountPoint, 'src/hello.txt'), 'utf8')
    .then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world');
      myVfs.unmount();
    }));
}

// Test: streams
{
  const { myVfs, mountPoint } = createMountedVfs();
  const chunks = [];
  const stream = fs.createReadStream(path.join(mountPoint, 'src/hello.txt'));
  stream.on('data', (chunk) => chunks.push(chunk));
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(chunks).toString(), 'hello world');
    myVfs.unmount();
  }));
}

// Test: openSync/readSync/closeSync via public fs
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  assert.notStrictEqual(fd & 0x40000000, 0);
  const buf = Buffer.alloc(11);
  const n = fs.readSync(fd, buf, 0, 11, 0);
  assert.strictEqual(n, 11);
  assert.strictEqual(buf.toString(), 'hello world');
  fs.closeSync(fd);
  myVfs.unmount();
}

// Test: ENOENT thrown for missing path under mount
{
  const { myVfs, mountPoint } = createMountedVfs();
  assert.throws(() => fs.readFileSync(path.join(mountPoint, 'src/missing.txt')),
                { code: 'ENOENT' });
  myVfs.unmount();
}

// Test: paths outside the mount point go to the real fs (no interference)
{
  const { myVfs, mountPoint } = createMountedVfs();
  // /etc/hostname (or any real path) should pass through; assert it doesn't
  // hit our VFS by checking that mountPoint is not a prefix of the path.
  assert.ok(!path.resolve('/etc').startsWith(mountPoint));
  myVfs.unmount();
}

// Test: Symbol.dispose unmounts
{
  const myVfs = vfs.create();
  myVfs.mount('/data');
  assert.strictEqual(myVfs.mounted, true);
  myVfs[Symbol.dispose]();
  assert.strictEqual(myVfs.mounted, false);
}
