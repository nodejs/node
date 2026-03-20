// Flags: --expose-internals
'use strict';

// Tests for follow-up items from nodejs/node#62328.
// Each section header references the plan item number.
// Each test uses a unique mount point to avoid overlap conflicts.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// --- 1.3 Overlapping mounts are rejected ---
{
  const vfs1 = vfs.create();
  vfs1.writeFileSync('/a.txt', 'a');
  vfs1.mount('/overlap_base');

  // Child of existing mount
  {
    const v = vfs.create();
    v.writeFileSync('/b.txt', 'b');
    assert.throws(() => v.mount('/overlap_base/sub'), {
      code: 'ERR_INVALID_STATE',
      message: /overlaps/,
    });
  }

  // Parent of existing mount
  {
    const v = vfs.create();
    v.writeFileSync('/b.txt', 'b');
    assert.throws(() => v.mount('/overlap'), {
      code: 'ERR_INVALID_STATE',
      message: /overlaps/,
    });
  }

  // Exact same mount point
  {
    const v = vfs.create();
    v.writeFileSync('/b.txt', 'b');
    assert.throws(() => v.mount('/overlap_base'), {
      code: 'ERR_INVALID_STATE',
      message: /overlaps/,
    });
  }

  vfs1.unmount();
}

// --- 2.1 VirtualFileHandle missing methods (no-op stubs) ---
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'data');
  myVfs.mount('/mnt_21');

  const fd = fs.openSync('/mnt_21/file.txt', 'r');
  assert.ok(fd >= 10000);
  fs.closeSync(fd);

  myVfs.unmount();
}

// --- 2.2 fsPromises.open() interception ---
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/hello.txt', 'hello from vfs');
  myVfs.mount('/mnt_22');

  (async () => {
    const fh = await fs.promises.open('/mnt_22/hello.txt', 'r');
    assert.ok(fh);
    const content = await fh.readFile('utf8');
    assert.strictEqual(content, 'hello from vfs');
    await fh.close();
    myVfs.unmount();
  })().then(common.mustCall());
}

// --- 2.3 Stream properties: bytesRead, bytesWritten, pending ---
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/stream.txt', 'stream data');

  const rs = myVfs.createReadStream('/stream.txt');
  assert.strictEqual(rs.pending, true);

  rs.on('data', common.mustCall(() => {
    assert.strictEqual(rs.pending, false);
    assert.ok(rs.bytesRead > 0);
  }));

  rs.on('end', common.mustCall());
}

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/out.txt', '');

  const ws = myVfs.createWriteStream('/out.txt');
  assert.strictEqual(ws.pending, true);
  assert.strictEqual(ws.bytesWritten, 0);

  ws.write('hello', common.mustCall(() => {
    assert.strictEqual(ws.pending, false);
    assert.strictEqual(ws.bytesWritten, 5);
    ws.end();
  }));
}

// --- 2.4 VirtualDir disposal ---
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.writeFileSync('/dir/a.txt', 'a');
  myVfs.mount('/mnt_24');

  // Symbol.dispose via closeSync
  const dir = fs.opendirSync('/mnt_24/dir');
  dir[Symbol.dispose]();
  assert.throws(() => dir.closeSync(), { code: 'ERR_DIR_CLOSED' });

  // Symbol.asyncDispose via close
  const dir2 = fs.opendirSync('/mnt_24/dir');
  dir2[Symbol.asyncDispose]().then(common.mustCall(() => {
    assert.throws(() => dir2.closeSync(), { code: 'ERR_DIR_CLOSED' });
  }));

  myVfs.unmount();
}

// --- 2.5 Stats ino/dev are non-zero ---
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/f1.txt', 'a');
  myVfs.writeFileSync('/f2.txt', 'b');

  const s1 = myVfs.statSync('/f1.txt');
  const s2 = myVfs.statSync('/f2.txt');

  // dev should be distinctive VFS value (4085)
  assert.strictEqual(s1.dev, 4085);
  assert.strictEqual(s2.dev, 4085);

  // ino should be unique per call
  assert.notStrictEqual(s1.ino, 0);
  assert.notStrictEqual(s2.ino, 0);
  assert.notStrictEqual(s1.ino, s2.ino);
}

// --- 3.3 writeFileSync respects append flags ---
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/append.txt', 'init');

  const fd = myVfs.openSync('/append.txt', 'a');
  const buf = Buffer.from(' more');
  myVfs.writeSync(fd, buf, 0, buf.length);
  myVfs.closeSync(fd);

  const content = myVfs.readFileSync('/append.txt', 'utf8');
  assert.strictEqual(content, 'init more');
}

// --- 3.4 Dead ternary in hookProcessCwd ---
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/f.txt', 'ok');
  myVfs.mount('/mnt_34');
  assert.strictEqual(fs.readFileSync('/mnt_34/f.txt', 'utf8'), 'ok');
  myVfs.unmount();
}

// --- 6.2 Cross-VFS operations throw EXDEV ---
{
  const vfs1 = vfs.create();
  vfs1.writeFileSync('/src.txt', 'data');
  vfs1.mount('/xdev_a');

  const vfs2 = vfs.create();
  vfs2.mkdirSync('/dest');
  vfs2.mount('/xdev_b');

  // rename across VFS mounts should throw EXDEV
  assert.throws(() => fs.renameSync('/xdev_a/src.txt', '/xdev_b/dest/src.txt'), {
    code: 'EXDEV',
  });

  // copyFile across VFS mounts should throw EXDEV
  assert.throws(() => fs.copyFileSync('/xdev_a/src.txt', '/xdev_b/dest/src.txt'), {
    code: 'EXDEV',
  });

  // link across VFS mounts should throw EXDEV
  assert.throws(() => fs.linkSync('/xdev_a/src.txt', '/xdev_b/dest/link.txt'), {
    code: 'EXDEV',
  });

  vfs2.unmount();
  vfs1.unmount();
}

// --- 8.1 MemoryProvider statSync size ---
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'abcdef');
  const st = myVfs.statSync('/file.txt');
  assert.strictEqual(st.size, 6);
}

// --- 8.3 Recursive readdir follows symlinks to directories ---
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/real-dir');
  myVfs.writeFileSync('/real-dir/nested.txt', 'nested');

  myVfs.mkdirSync('/root');
  myVfs.symlinkSync('/real-dir', '/root/symdir');

  const entries = myVfs.readdirSync('/root', { recursive: true });
  assert.ok(entries.includes('symdir'));
  assert.ok(
    entries.includes('symdir/nested.txt'),
    `Expected 'symdir/nested.txt' in entries: ${entries}`,
  );
}

// --- 6.3 Package.json cache is cleared on unmount ---
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg');
  myVfs.writeFileSync('/pkg/package.json', '{"name":"test","type":"module"}');
  myVfs.writeFileSync('/pkg/index.js', 'module.exports = 42');
  myVfs.mount('/mnt_63');

  // Access the file so caches are populated
  assert.ok(fs.existsSync('/mnt_63/pkg/package.json'));

  // After unmount, cache should be cleared (no stale entries)
  myVfs.unmount();

  assert.strictEqual(fs.existsSync('/mnt_63/pkg/package.json'), false);
}

// --- 5.1 readFile async handler (not sync) ---
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/async-read.txt', 'async content');
  myVfs.mount('/mnt_51');

  fs.readFile('/mnt_51/async-read.txt', 'utf8', common.mustCall((err, data) => {
    assert.ifError(err);
    assert.strictEqual(data, 'async content');
    myVfs.unmount();
  }));
}
