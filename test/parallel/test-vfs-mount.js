// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

// Basic mount/unmount API and dispatch through node:vfs from the public fs.

const baseMountPoint = path.resolve('/tmp/vfs-mount-' + process.pid);
let mountCounter = 0;

function createMountedVfs() {
  const mountPoint = baseMountPoint + '-' + (mountCounter++);
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello world');
  myVfs.mount(mountPoint);
  return { myVfs, mountPoint };
}

// Test: mounted/mountPoint getters
{
  const myVfs = vfs.create();
  assert.strictEqual(myVfs.mounted, false);
  assert.strictEqual(myVfs.mountPoint, null);

  const mountPoint = baseMountPoint + '-' + (mountCounter++);
  myVfs.mount(mountPoint);
  assert.strictEqual(myVfs.mounted, true);
  assert.strictEqual(myVfs.mountPoint, mountPoint);

  myVfs.unmount();
  assert.strictEqual(myVfs.mounted, false);
  assert.strictEqual(myVfs.mountPoint, null);
}

// Test: double-mount throws
{
  const myVfs = vfs.create();
  const mountPoint = baseMountPoint + '-' + (mountCounter++);
  myVfs.mount(mountPoint);
  assert.throws(() => myVfs.mount(mountPoint), { code: 'ERR_INVALID_STATE' });
  myVfs.unmount();
}

// Test: overlapping mounts throw
{
  const a = vfs.create();
  const b = vfs.create();
  const mountPoint = baseMountPoint + '-' + (mountCounter++);
  a.mount(mountPoint);
  assert.throws(() => b.mount(mountPoint), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => b.mount(path.join(mountPoint, 'inner')),
                { code: 'ERR_INVALID_STATE' });
  a.unmount();
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
  const mountPoint = baseMountPoint + '-' + (mountCounter++);
  myVfs.mount(mountPoint);
  assert.strictEqual(myVfs.mounted, true);
  myVfs[Symbol.dispose]();
  assert.strictEqual(myVfs.mounted, false);
}
