'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const baseMountPoint = path.resolve('/tmp/vfs-abort-' + process.pid);
let mountCounter = 0;

function createMountedVfs() {
  const mountPoint = baseMountPoint + '-' + (mountCounter++);
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/file.txt', 'hello');
  myVfs.mount(mountPoint);
  return { myVfs, mountPoint };
}

// Test fs.readFile with pre-aborted signal on VFS path
{
  const { myVfs, mountPoint } = createMountedVfs();
  const ac = new AbortController();
  ac.abort();
  fs.readFile(
    path.join(mountPoint, 'src/file.txt'),
    { signal: ac.signal },
    common.mustCall((err, data) => {
      assert.strictEqual(err.name, 'AbortError');
      assert.strictEqual(data, undefined);
      myVfs.unmount();
    }),
  );
}

// Test fs.writeFile with pre-aborted signal on VFS path
{
  const { myVfs, mountPoint } = createMountedVfs();
  const ac = new AbortController();
  ac.abort();
  fs.writeFile(
    path.join(mountPoint, 'src/file.txt'),
    'new data',
    { signal: ac.signal },
    common.mustCall((err) => {
      assert.strictEqual(err.name, 'AbortError');
      // Content should NOT have been written
      const content = fs.readFileSync(
        path.join(mountPoint, 'src/file.txt'), 'utf8');
      assert.strictEqual(content, 'hello');
      myVfs.unmount();
    }),
  );
}

// Test fs.appendFile with pre-aborted signal on VFS path
{
  const { myVfs, mountPoint } = createMountedVfs();
  const ac = new AbortController();
  ac.abort();
  fs.appendFile(
    path.join(mountPoint, 'src/file.txt'),
    ' world',
    { signal: ac.signal },
    common.mustCall((err) => {
      assert.strictEqual(err.name, 'AbortError');
      const content = fs.readFileSync(
        path.join(mountPoint, 'src/file.txt'), 'utf8');
      assert.strictEqual(content, 'hello');
      myVfs.unmount();
    }),
  );
}

// Test fs.promises.readFile with pre-aborted signal on VFS path
{
  const { myVfs, mountPoint } = createMountedVfs();
  const ac = new AbortController();
  ac.abort();
  assert.rejects(
    fs.promises.readFile(
      path.join(mountPoint, 'src/file.txt'),
      { signal: ac.signal },
    ),
    { name: 'AbortError' },
  ).then(() => myVfs.unmount()).then(common.mustCall());
}

// Test fs.promises.writeFile with pre-aborted signal on VFS path
{
  const { myVfs, mountPoint } = createMountedVfs();
  const ac = new AbortController();
  ac.abort();
  assert.rejects(
    fs.promises.writeFile(
      path.join(mountPoint, 'src/file.txt'),
      'new data',
      { signal: ac.signal },
    ),
    { name: 'AbortError' },
  ).then(() => myVfs.unmount()).then(common.mustCall());
}

// Test fs.promises.appendFile with pre-aborted signal on VFS path
{
  const { myVfs, mountPoint } = createMountedVfs();
  const ac = new AbortController();
  ac.abort();
  assert.rejects(
    fs.promises.appendFile(
      path.join(mountPoint, 'src/file.txt'),
      ' world',
      { signal: ac.signal },
    ),
    { name: 'AbortError' },
  ).then(() => myVfs.unmount()).then(common.mustCall());
}
