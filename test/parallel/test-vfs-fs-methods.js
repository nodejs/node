// Flags: --expose-internals
'use strict';

// Tests that VFS intercepts all supported fs methods when mounted.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const baseMountPoint = path.resolve('/tmp/vfs-test-' + process.pid);
let mountCounter = 0;

// Helper to create a mounted VFS with test content.
// Each call uses a unique mount point to avoid interference between
// async test blocks that may run concurrently.
function createMountedVfs() {
  const mountPoint = baseMountPoint + '-' + (mountCounter++);
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.mkdirSync('/src/subdir', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello world');
  myVfs.writeFileSync('/src/data.json', '{"key":"value"}');
  myVfs.mount(mountPoint);
  return { myVfs, mountPoint };
}

// ==================== Sync read ops ====================

// Test fs.existsSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), true);
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'nonexistent')), false);
  myVfs.unmount();
}

// Test fs.statSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const stats = fs.statSync(path.join(mountPoint, 'src/hello.txt'));
  assert.strictEqual(stats.isFile(), true);
  assert.strictEqual(stats.size, 11);
  myVfs.unmount();
}

// Test fs.lstatSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const stats = fs.lstatSync(path.join(mountPoint, 'src/hello.txt'));
  assert.strictEqual(stats.isFile(), true);
  myVfs.unmount();
}

// Test fs.readFileSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
  assert.strictEqual(content, 'hello world');
  myVfs.unmount();
}

// Test fs.readdirSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const entries = fs.readdirSync(path.join(mountPoint, 'src'));
  assert.ok(entries.includes('hello.txt'));
  assert.ok(entries.includes('data.json'));
  assert.ok(entries.includes('subdir'));
  myVfs.unmount();
}

// Test fs.realpathSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const rp = fs.realpathSync(path.join(mountPoint, 'src/hello.txt'));
  assert.strictEqual(rp, path.join(mountPoint, 'src/hello.txt'));
  myVfs.unmount();
}

// Test fs.accessSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  // Should not throw for existing file
  fs.accessSync(path.join(mountPoint, 'src/hello.txt'));
  // Should throw for non-existing file
  assert.throws(() => {
    fs.accessSync(path.join(mountPoint, 'nonexistent'));
  }, { code: 'ENOENT' });
  myVfs.unmount();
}

// ==================== Sync write ops ====================

// Test fs.writeFileSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.writeFileSync(path.join(mountPoint, 'src/new.txt'), 'new content');
  const content = fs.readFileSync(path.join(mountPoint, 'src/new.txt'), 'utf8');
  assert.strictEqual(content, 'new content');
  myVfs.unmount();
}

// Test fs.appendFileSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.appendFileSync(path.join(mountPoint, 'src/hello.txt'), ' appended');
  const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
  assert.strictEqual(content, 'hello world appended');
  myVfs.unmount();
}

// Test fs.mkdirSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.mkdirSync(path.join(mountPoint, 'src/newdir'));
  const stats = fs.statSync(path.join(mountPoint, 'src/newdir'));
  assert.strictEqual(stats.isDirectory(), true);
  myVfs.unmount();
}

// Test fs.mkdirSync recursive on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const result = fs.mkdirSync(path.join(mountPoint, 'src/a/b/c'), { recursive: true });
  assert.ok(result !== undefined);
  const stats = fs.statSync(path.join(mountPoint, 'src/a/b/c'));
  assert.strictEqual(stats.isDirectory(), true);
  myVfs.unmount();
}

// Test fs.unlinkSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), true);
  fs.unlinkSync(path.join(mountPoint, 'src/hello.txt'));
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false);
  myVfs.unmount();
}

// Test fs.rmdirSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.mkdirSync(path.join(mountPoint, 'src/emptydir'));
  fs.rmdirSync(path.join(mountPoint, 'src/emptydir'));
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/emptydir')), false);
  myVfs.unmount();
}

// Test fs.rmSync on VFS (file)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.rmSync(path.join(mountPoint, 'src/hello.txt'));
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false);
  myVfs.unmount();
}

// Test fs.rmSync on VFS (recursive directory)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.rmSync(path.join(mountPoint, 'src'), { recursive: true });
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src')), false);
  myVfs.unmount();
}

// Test fs.renameSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.renameSync(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/renamed.txt'),
  );
  assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false);
  const content = fs.readFileSync(path.join(mountPoint, 'src/renamed.txt'), 'utf8');
  assert.strictEqual(content, 'hello world');
  myVfs.unmount();
}

// Test fs.copyFileSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.copyFileSync(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/copy.txt'),
  );
  const content = fs.readFileSync(path.join(mountPoint, 'src/copy.txt'), 'utf8');
  assert.strictEqual(content, 'hello world');
  myVfs.unmount();
}

// ==================== FD-based sync ops ====================

// Test fs.openSync/readSync/writeSync/fstatSync/closeSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();

  // Open for reading
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  assert.ok(fd >= 10000); // VFS FDs start at 10000

  // fstat
  const stats = fs.fstatSync(fd);
  assert.strictEqual(stats.isFile(), true);
  assert.strictEqual(stats.size, 11);

  // Read
  const buf = Buffer.alloc(11);
  const bytesRead = fs.readSync(fd, buf, 0, 11, 0);
  assert.strictEqual(bytesRead, 11);
  assert.strictEqual(buf.toString(), 'hello world');

  // Close
  fs.closeSync(fd);
  myVfs.unmount();
}

// Test FD write operations
{
  const { myVfs, mountPoint } = createMountedVfs();

  const fd = fs.openSync(path.join(mountPoint, 'src/writeme.txt'), 'w');
  assert.ok(fd >= 10000);

  const data = Buffer.from('written via fd');
  const bytesWritten = fs.writeSync(fd, data, 0, data.length, 0);
  assert.strictEqual(bytesWritten, data.length);

  fs.closeSync(fd);

  const content = fs.readFileSync(path.join(mountPoint, 'src/writeme.txt'), 'utf8');
  assert.strictEqual(content, 'written via fd');
  myVfs.unmount();
}

// ==================== Callback-based async read ops ====================

// Test fs.stat callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.stat(path.join(mountPoint, 'src/hello.txt'), common.mustCall((err, stats) => {
    assert.strictEqual(err, null);
    assert.strictEqual(stats.isFile(), true);
    assert.strictEqual(stats.size, 11);
    myVfs.unmount();
  }));
}

// Test fs.lstat callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.lstat(path.join(mountPoint, 'src/hello.txt'), common.mustCall((err, stats) => {
    assert.strictEqual(err, null);
    assert.strictEqual(stats.isFile(), true);
    myVfs.unmount();
  }));
}

// Test fs.readFile callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.readFile(path.join(mountPoint, 'src/hello.txt'), 'utf8', common.mustCall((err, data) => {
    assert.strictEqual(err, null);
    assert.strictEqual(data, 'hello world');
    myVfs.unmount();
  }));
}

// Test fs.readdir callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.readdir(path.join(mountPoint, 'src'), common.mustCall((err, entries) => {
    assert.strictEqual(err, null);
    assert.ok(entries.includes('hello.txt'));
    myVfs.unmount();
  }));
}

// Test fs.realpath callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.realpath(path.join(mountPoint, 'src/hello.txt'), common.mustCall((err, rp) => {
    assert.strictEqual(err, null);
    assert.strictEqual(rp, path.join(mountPoint, 'src/hello.txt'));
    myVfs.unmount();
  }));
}

// Test fs.access callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.access(path.join(mountPoint, 'src/hello.txt'), common.mustCall((err) => {
    assert.strictEqual(err, null);
    myVfs.unmount();
  }));
}

// Test fs.exists callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.exists(path.join(mountPoint, 'src/hello.txt'), common.mustCall((exists) => {
    assert.strictEqual(exists, true);
    fs.exists(path.join(mountPoint, 'nonexistent'), common.mustCall((exists2) => {
      assert.strictEqual(exists2, false);
      myVfs.unmount();
    }));
  }));
}

// ==================== Callback-based async write ops ====================

// Test fs.writeFile callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.writeFile(path.join(mountPoint, 'src/cb-write.txt'), 'callback data', common.mustCall((err) => {
    assert.strictEqual(err, null);
    const content = fs.readFileSync(path.join(mountPoint, 'src/cb-write.txt'), 'utf8');
    assert.strictEqual(content, 'callback data');
    myVfs.unmount();
  }));
}

// Test fs.mkdir callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.mkdir(path.join(mountPoint, 'src/cb-dir'), common.mustCall((err) => {
    assert.strictEqual(err, null);
    const stats = fs.statSync(path.join(mountPoint, 'src/cb-dir'));
    assert.strictEqual(stats.isDirectory(), true);
    myVfs.unmount();
  }));
}

// Test fs.unlink callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.unlink(path.join(mountPoint, 'src/hello.txt'), common.mustCall((err) => {
    assert.strictEqual(err, null);
    assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false);
    myVfs.unmount();
  }));
}

// Test fs.rename callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.rename(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/renamed-cb.txt'),
    common.mustCall((err) => {
      assert.strictEqual(err, null);
      assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false);
      const content = fs.readFileSync(path.join(mountPoint, 'src/renamed-cb.txt'), 'utf8');
      assert.strictEqual(content, 'hello world');
      myVfs.unmount();
    }),
  );
}

// Test fs.copyFile callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.copyFile(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/copy-cb.txt'),
    common.mustCall((err) => {
      assert.strictEqual(err, null);
      const content = fs.readFileSync(path.join(mountPoint, 'src/copy-cb.txt'), 'utf8');
      assert.strictEqual(content, 'hello world');
      myVfs.unmount();
    }),
  );
}

// Test fs.open/read/write/fstat/close callbacks on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.open(path.join(mountPoint, 'src/hello.txt'), 'r', common.mustCall((err, fd) => {
    assert.strictEqual(err, null);
    assert.ok(fd >= 10000);

    fs.fstat(fd, common.mustCall((err2, stats) => {
      assert.strictEqual(err2, null);
      assert.strictEqual(stats.isFile(), true);

      const buf = Buffer.alloc(11);
      fs.read(fd, buf, 0, 11, 0, common.mustCall((err3, bytesRead, buffer) => {
        assert.strictEqual(err3, null);
        assert.strictEqual(bytesRead, 11);
        assert.strictEqual(buffer.toString(), 'hello world');

        fs.close(fd, common.mustCall((err4) => {
          assert.strictEqual(err4, null);
          myVfs.unmount();
        }));
      }));
    }));
  }));
}

// ==================== Promise-based async ops ====================

// Test fs.promises.stat on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.stat(path.join(mountPoint, 'src/hello.txt')).then(common.mustCall((stats) => {
    assert.strictEqual(stats.isFile(), true);
    assert.strictEqual(stats.size, 11);
    myVfs.unmount();
  }));
}

// Test fs.promises.lstat on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.lstat(path.join(mountPoint, 'src/hello.txt')).then(common.mustCall((stats) => {
    assert.strictEqual(stats.isFile(), true);
    myVfs.unmount();
  }));
}

// Test fs.promises.readFile on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.readFile(path.join(mountPoint, 'src/hello.txt'), 'utf8').then(common.mustCall((data) => {
    assert.strictEqual(data, 'hello world');
    myVfs.unmount();
  }));
}

// Test fs.promises.readdir on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.readdir(path.join(mountPoint, 'src')).then(common.mustCall((entries) => {
    assert.ok(entries.includes('hello.txt'));
    myVfs.unmount();
  }));
}

// Test fs.promises.realpath on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.realpath(path.join(mountPoint, 'src/hello.txt')).then(common.mustCall((rp) => {
    assert.strictEqual(rp, path.join(mountPoint, 'src/hello.txt'));
    myVfs.unmount();
  }));
}

// Test fs.promises.access on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.access(path.join(mountPoint, 'src/hello.txt')).then(common.mustCall(() => {
    myVfs.unmount();
  }));
}

// Test fs.promises.writeFile on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.writeFile(path.join(mountPoint, 'src/promise-write.txt'), 'promise data').then(common.mustCall(() => {
    const content = fs.readFileSync(path.join(mountPoint, 'src/promise-write.txt'), 'utf8');
    assert.strictEqual(content, 'promise data');
    myVfs.unmount();
  }));
}

// Test fs.promises.appendFile on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.appendFile(path.join(mountPoint, 'src/hello.txt'), ' appended').then(common.mustCall(() => {
    const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
    assert.strictEqual(content, 'hello world appended');
    myVfs.unmount();
  }));
}

// Test fs.promises.mkdir on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.mkdir(path.join(mountPoint, 'src/promise-dir')).then(common.mustCall(() => {
    const stats = fs.statSync(path.join(mountPoint, 'src/promise-dir'));
    assert.strictEqual(stats.isDirectory(), true);
    myVfs.unmount();
  }));
}

// Test fs.promises.rmdir on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.mkdirSync(path.join(mountPoint, 'src/rmdir-test'));
  fs.promises.rmdir(path.join(mountPoint, 'src/rmdir-test')).then(common.mustCall(() => {
    assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/rmdir-test')), false);
    myVfs.unmount();
  }));
}

// Test fs.promises.unlink on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.unlink(path.join(mountPoint, 'src/hello.txt')).then(common.mustCall(() => {
    assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false);
    myVfs.unmount();
  }));
}

// Test fs.promises.rename on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.rename(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/renamed-promise.txt'),
  ).then(common.mustCall(() => {
    assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false);
    const content = fs.readFileSync(path.join(mountPoint, 'src/renamed-promise.txt'), 'utf8');
    assert.strictEqual(content, 'hello world');
    myVfs.unmount();
  }));
}

// Test fs.promises.copyFile on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.copyFile(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/copy-promise.txt'),
  ).then(common.mustCall(() => {
    const content = fs.readFileSync(path.join(mountPoint, 'src/copy-promise.txt'), 'utf8');
    assert.strictEqual(content, 'hello world');
    myVfs.unmount();
  }));
}

// Test fs.promises.rm on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.rm(path.join(mountPoint, 'src/hello.txt')).then(common.mustCall(() => {
    assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false);
    myVfs.unmount();
  }));
}

// ==================== Stream ops ====================

// Test fs.createReadStream on VFS
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

// Test fs.createWriteStream on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const stream = fs.createWriteStream(path.join(mountPoint, 'src/stream-write.txt'));
  stream.write('stream ');
  stream.end('data', common.mustCall(() => {
    const content = fs.readFileSync(path.join(mountPoint, 'src/stream-write.txt'), 'utf8');
    assert.strictEqual(content, 'stream data');
    myVfs.unmount();
  }));
}
