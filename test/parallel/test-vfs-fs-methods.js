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

// ==================== Symlink ops ====================

// Test fs.symlinkSync / fs.readlinkSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.symlinkSync('hello.txt', path.join(mountPoint, 'src/link.txt'));
  const target = fs.readlinkSync(path.join(mountPoint, 'src/link.txt'));
  assert.strictEqual(target, 'hello.txt');
  myVfs.unmount();
}

// Test fs.symlink / fs.readlink callbacks on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.symlink('hello.txt', path.join(mountPoint, 'src/link-cb.txt'), common.mustCall((err) => {
    assert.strictEqual(err, null);
    fs.readlink(path.join(mountPoint, 'src/link-cb.txt'), common.mustCall((err2, target) => {
      assert.strictEqual(err2, null);
      assert.strictEqual(target, 'hello.txt');
      myVfs.unmount();
    }));
  }));
}

// Test fs.promises.symlink / fs.promises.readlink on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.symlink('hello.txt', path.join(mountPoint, 'src/link-promise.txt'))
    .then(() => fs.promises.readlink(path.join(mountPoint, 'src/link-promise.txt')))
    .then(common.mustCall((target) => {
      assert.strictEqual(target, 'hello.txt');
      myVfs.unmount();
    }));
}

// ==================== Additional callback write ops ====================

// Test fs.appendFile callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.appendFile(path.join(mountPoint, 'src/hello.txt'), ' cb-appended', common.mustCall((err) => {
    assert.strictEqual(err, null);
    const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
    assert.strictEqual(content, 'hello world cb-appended');
    myVfs.unmount();
  }));
}

// Test fs.rmdir callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.mkdirSync(path.join(mountPoint, 'src/cb-rmdir-test'));
  fs.rmdir(path.join(mountPoint, 'src/cb-rmdir-test'), common.mustCall((err) => {
    assert.strictEqual(err, null);
    assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/cb-rmdir-test')), false);
    myVfs.unmount();
  }));
}

// Test fs.rm callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.rm(path.join(mountPoint, 'src/hello.txt'), common.mustCall((err) => {
    assert.strictEqual(err, null);
    assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false);
    myVfs.unmount();
  }));
}

// ==================== FD callback write ops ====================

// Test fs.write callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.open(path.join(mountPoint, 'src/fd-write-cb.txt'), 'w', common.mustCall((err, fd) => {
    assert.strictEqual(err, null);
    const data = Buffer.from('fd write callback');
    fs.write(fd, data, 0, data.length, 0, common.mustCall((err2, bytesWritten) => {
      assert.strictEqual(err2, null);
      assert.strictEqual(bytesWritten, data.length);
      fs.close(fd, common.mustCall((err3) => {
        assert.strictEqual(err3, null);
        const content = fs.readFileSync(path.join(mountPoint, 'src/fd-write-cb.txt'), 'utf8');
        assert.strictEqual(content, 'fd write callback');
        myVfs.unmount();
      }));
    }));
  }));
}

// ==================== writeSync with string argument ====================

// Test fs.writeSync with string (not Buffer)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/string-write.txt'), 'w');
  const bytesWritten = fs.writeSync(fd, 'string data', 0, 'utf8');
  assert.ok(bytesWritten > 0);
  fs.closeSync(fd);
  const content = fs.readFileSync(path.join(mountPoint, 'src/string-write.txt'), 'utf8');
  assert.strictEqual(content, 'string data');
  myVfs.unmount();
}

// ==================== ReadStream with start/end options ====================

// Test fs.createReadStream with start/end on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const chunks = [];
  const stream = fs.createReadStream(path.join(mountPoint, 'src/hello.txt'), {
    start: 0,
    end: 4,
  });
  assert.strictEqual(stream.path, path.join(mountPoint, 'src/hello.txt'));
  stream.on('data', (chunk) => chunks.push(chunk));
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(chunks).toString(), 'hello');
    myVfs.unmount();
  }));
}

// ==================== Stream open event ====================

// Test ReadStream emits 'open' with VFS fd
{
  const { myVfs, mountPoint } = createMountedVfs();
  const stream = fs.createReadStream(path.join(mountPoint, 'src/hello.txt'));
  stream.on('open', common.mustCall((fd) => {
    assert.ok(fd >= 10000);
  }));
  stream.on('end', common.mustCall(() => {
    myVfs.unmount();
  }));
  stream.resume(); // Consume the stream
}

// Test WriteStream path getter and 'open' event
{
  const { myVfs, mountPoint } = createMountedVfs();
  const filePath = path.join(mountPoint, 'src/ws-open.txt');
  const stream = fs.createWriteStream(filePath);
  assert.strictEqual(stream.path, filePath);
  stream.on('open', common.mustCall((fd) => {
    assert.ok(fd >= 10000);
  }));
  stream.end('done', common.mustCall(() => {
    myVfs.unmount();
  }));
}

// ==================== VFS class properties ====================

// Test VFS instance property getters
{
  const myVfs = vfs.create();
  assert.ok(myVfs.provider !== null);
  assert.strictEqual(myVfs.mountPoint, null);
  assert.strictEqual(myVfs.mounted, false);
  assert.strictEqual(myVfs.readonly, false);
  assert.strictEqual(myVfs.overlay, false);

  const mountPoint = baseMountPoint + '-' + (mountCounter++);
  myVfs.mount(mountPoint);
  assert.strictEqual(myVfs.mountPoint, mountPoint);
  assert.strictEqual(myVfs.mounted, true);
  myVfs.unmount();
  assert.strictEqual(myVfs.mountPoint, null);
  assert.strictEqual(myVfs.mounted, false);
}

// ==================== rmSync with force option ====================

// Test fs.rmSync with force: true on nonexistent file
{
  const { myVfs, mountPoint } = createMountedVfs();
  // Should not throw with force: true
  fs.rmSync(path.join(mountPoint, 'nonexistent'), { force: true });
  myVfs.unmount();
}

// ==================== promises.rm recursive ====================

// Test fs.promises.rm with recursive directory
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.rm(path.join(mountPoint, 'src'), { recursive: true }).then(common.mustCall(() => {
    assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src')), false);
    myVfs.unmount();
  }));
}

// ==================== truncate ops ====================

// Test fs.truncateSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.truncateSync(path.join(mountPoint, 'src/hello.txt'), 5);
  const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
  assert.strictEqual(content, 'hello');
  myVfs.unmount();
}

// Test fs.truncate callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.truncate(path.join(mountPoint, 'src/hello.txt'), 5, common.mustCall((err) => {
    assert.strictEqual(err, null);
    const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
    assert.strictEqual(content, 'hello');
    myVfs.unmount();
  }));
}

// Test fs.promises.truncate on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.truncate(path.join(mountPoint, 'src/hello.txt'), 5).then(common.mustCall(() => {
    const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
    assert.strictEqual(content, 'hello');
    myVfs.unmount();
  }));
}

// ==================== ftruncate ops ====================

// Test fs.ftruncateSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r+');
  fs.ftruncateSync(fd, 5);
  fs.closeSync(fd);
  const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
  assert.strictEqual(content, 'hello');
  myVfs.unmount();
}

// Test fs.ftruncate callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r+');
  fs.ftruncate(fd, 5, common.mustCall((err) => {
    assert.strictEqual(err, null);
    fs.closeSync(fd);
    const content = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8');
    assert.strictEqual(content, 'hello');
    myVfs.unmount();
  }));
}

// ==================== link ops ====================

// Test fs.linkSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.linkSync(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/hello-link.txt'),
  );
  const content = fs.readFileSync(path.join(mountPoint, 'src/hello-link.txt'), 'utf8');
  assert.strictEqual(content, 'hello world');
  myVfs.unmount();
}

// Test fs.link callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.link(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/hello-link-cb.txt'),
    common.mustCall((err) => {
      assert.strictEqual(err, null);
      const content = fs.readFileSync(path.join(mountPoint, 'src/hello-link-cb.txt'), 'utf8');
      assert.strictEqual(content, 'hello world');
      myVfs.unmount();
    }),
  );
}

// Test fs.promises.link on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.link(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/hello-link-promise.txt'),
  ).then(common.mustCall(() => {
    const content = fs.readFileSync(path.join(mountPoint, 'src/hello-link-promise.txt'), 'utf8');
    assert.strictEqual(content, 'hello world');
    myVfs.unmount();
  }));
}

// ==================== mkdtemp ops ====================

// Test fs.mkdtempSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const tmpdir = fs.mkdtempSync(path.join(mountPoint, 'src/tmp-'));
  assert.ok(tmpdir.startsWith(path.join(mountPoint, 'src/tmp-')));
  assert.strictEqual(tmpdir.length, path.join(mountPoint, 'src/tmp-').length + 6);
  assert.strictEqual(fs.statSync(tmpdir).isDirectory(), true);
  myVfs.unmount();
}

// Test fs.mkdtemp callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.mkdtemp(path.join(mountPoint, 'src/tmp-'), common.mustCall((err, tmpdir) => {
    assert.strictEqual(err, null);
    assert.ok(tmpdir.startsWith(path.join(mountPoint, 'src/tmp-')));
    assert.strictEqual(fs.statSync(tmpdir).isDirectory(), true);
    myVfs.unmount();
  }));
}

// Test fs.promises.mkdtemp on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.mkdtemp(path.join(mountPoint, 'src/tmp-')).then(common.mustCall((tmpdir) => {
    assert.ok(tmpdir.startsWith(path.join(mountPoint, 'src/tmp-')));
    assert.strictEqual(fs.statSync(tmpdir).isDirectory(), true);
    myVfs.unmount();
  }));
}

// ==================== opendir ops ====================

// Test fs.opendirSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const dir = fs.opendirSync(path.join(mountPoint, 'src'));
  const names = [];
  let entry;
  while ((entry = dir.readSync()) !== null) {
    names.push(entry.name);
  }
  dir.closeSync();
  assert.ok(names.includes('hello.txt'));
  assert.ok(names.includes('data.json'));
  assert.ok(names.includes('subdir'));
  myVfs.unmount();
}

// Test fs.opendir callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.opendir(path.join(mountPoint, 'src'), common.mustCall((err, dir) => {
    assert.strictEqual(err, null);
    const names = [];
    let entry;
    while ((entry = dir.readSync()) !== null) {
      names.push(entry.name);
    }
    dir.closeSync();
    assert.ok(names.includes('hello.txt'));
    myVfs.unmount();
  }));
}

// Test opendir async iteration on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.opendir(path.join(mountPoint, 'src'), common.mustCall(async (err, dir) => {
    assert.strictEqual(err, null);
    const names = [];
    for await (const entry of dir) {
      names.push(entry.name);
    }
    assert.ok(names.includes('hello.txt'));
    assert.ok(names.includes('data.json'));
    myVfs.unmount();
  }));
}

// ==================== openAsBlob ====================

// Test fs.openAsBlob on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.openAsBlob(path.join(mountPoint, 'src/hello.txt')).then(async (blob) => {
    assert.ok(blob instanceof Blob);
    assert.strictEqual(blob.size, 11);
    const text = await blob.text();
    assert.strictEqual(text, 'hello world');
    myVfs.unmount();
  }).then(common.mustCall());
}

// Test fs.openAsBlob with type option on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.openAsBlob(path.join(mountPoint, 'src/data.json'), { type: 'application/json' })
    .then(async (blob) => {
      assert.strictEqual(blob.type, 'application/json');
      const text = await blob.text();
      assert.strictEqual(text, '{"key":"value"}');
      myVfs.unmount();
    }).then(common.mustCall());
}

// ==================== cp ops ====================

// Test fs.cpSync on VFS (directory tree)
{
  const { myVfs, mountPoint } = createMountedVfs();
  // Add some content to subdir
  fs.writeFileSync(path.join(mountPoint, 'src/subdir/nested.txt'), 'nested content');
  fs.cpSync(
    path.join(mountPoint, 'src'),
    path.join(mountPoint, 'src-copy'),
    { recursive: true },
  );
  assert.strictEqual(
    fs.readFileSync(path.join(mountPoint, 'src-copy/hello.txt'), 'utf8'),
    'hello world',
  );
  assert.strictEqual(
    fs.readFileSync(path.join(mountPoint, 'src-copy/subdir/nested.txt'), 'utf8'),
    'nested content',
  );
  myVfs.unmount();
}

// Test fs.cp callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.cp(
    path.join(mountPoint, 'src'),
    path.join(mountPoint, 'src-copy-cb'),
    { recursive: true },
    common.mustCall((err) => {
      assert.strictEqual(err, null);
      assert.strictEqual(
        fs.readFileSync(path.join(mountPoint, 'src-copy-cb/hello.txt'), 'utf8'),
        'hello world',
      );
      myVfs.unmount();
    }),
  );
}

// Test fs.promises.cp on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.cp(
    path.join(mountPoint, 'src'),
    path.join(mountPoint, 'src-copy-promise'),
    { recursive: true },
  ).then(common.mustCall(() => {
    assert.strictEqual(
      fs.readFileSync(path.join(mountPoint, 'src-copy-promise/hello.txt'), 'utf8'),
      'hello world',
    );
    myVfs.unmount();
  }));
}

// ==================== glob ops ====================

// Test fs.globSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const matches = fs.globSync(path.join(mountPoint, 'src/*.txt'));
  assert.ok(matches.length >= 1);
  assert.ok(matches.some((m) => m.endsWith('hello.txt')));
  myVfs.unmount();
}

// Test fs.glob callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.glob(path.join(mountPoint, 'src/*.txt'), common.mustCall((err, result) => {
    assert.strictEqual(err, null);
    assert.ok(result.length >= 1);
    assert.ok(result.some((m) => m.endsWith('hello.txt')));
    myVfs.unmount();
  }));
}

// Test fs.promises.glob async iterator on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  (async () => {
    const matches = [];
    for await (const match of fs.promises.glob(path.join(mountPoint, 'src/*.txt'))) {
      matches.push(match);
    }
    assert.ok(matches.length >= 1);
    assert.ok(matches.some((m) => m.endsWith('hello.txt')));
    myVfs.unmount();
  })().then(common.mustCall());
}

// ==================== chown ops ====================

// Test fs.chownSync on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.chownSync(path.join(mountPoint, 'src/hello.txt'), 1000, 1000);
  // File should still be readable
  assert.strictEqual(fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8'), 'hello world');
  myVfs.unmount();
}

// Test fs.chown callback on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.chown(path.join(mountPoint, 'src/hello.txt'), 1000, 1000, common.mustCall((err) => {
    assert.strictEqual(err, null);
    myVfs.unmount();
  }));
}

// Test fs.promises.chown on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.chown(path.join(mountPoint, 'src/hello.txt'), 1000, 1000).then(common.mustCall(() => {
    myVfs.unmount();
  }));
}

// ==================== lchown ops ====================

// Test fs.lchownSync on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.lchownSync(path.join(mountPoint, 'src/hello.txt'), 1000, 1000);
  assert.strictEqual(fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8'), 'hello world');
  myVfs.unmount();
}

// Test fs.lchown callback on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.lchown(path.join(mountPoint, 'src/hello.txt'), 1000, 1000, common.mustCall((err) => {
    assert.strictEqual(err, null);
    myVfs.unmount();
  }));
}

// Test fs.promises.lchown on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.lchown(path.join(mountPoint, 'src/hello.txt'), 1000, 1000).then(common.mustCall(() => {
    myVfs.unmount();
  }));
}

// ==================== fchmod ops ====================

// Test fs.fchmodSync on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.fchmodSync(fd, 0o644);
  fs.closeSync(fd);
  myVfs.unmount();
}

// Test fs.fchmod callback on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.fchmod(fd, 0o644, common.mustCall((err) => {
    assert.strictEqual(err, null);
    fs.closeSync(fd);
    myVfs.unmount();
  }));
}

// ==================== fchown ops ====================

// Test fs.fchownSync on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.fchownSync(fd, 1000, 1000);
  fs.closeSync(fd);
  myVfs.unmount();
}

// Test fs.fchown callback on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.fchown(fd, 1000, 1000, common.mustCall((err) => {
    assert.strictEqual(err, null);
    fs.closeSync(fd);
    myVfs.unmount();
  }));
}

// ==================== futimes ops ====================

// Test fs.futimesSync on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.futimesSync(fd, Date.now() / 1000, Date.now() / 1000);
  fs.closeSync(fd);
  myVfs.unmount();
}

// Test fs.futimes callback on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.futimes(fd, Date.now() / 1000, Date.now() / 1000, common.mustCall((err) => {
    assert.strictEqual(err, null);
    fs.closeSync(fd);
    myVfs.unmount();
  }));
}

// ==================== lutimes ops ====================

// Test fs.lutimesSync on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.lutimesSync(path.join(mountPoint, 'src/hello.txt'), Date.now() / 1000, Date.now() / 1000);
  assert.strictEqual(fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8'), 'hello world');
  myVfs.unmount();
}

// Test fs.lutimes callback on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.lutimes(path.join(mountPoint, 'src/hello.txt'),
             Date.now() / 1000,
             Date.now() / 1000,
             common.mustCall((err) => {
               assert.strictEqual(err, null);
               myVfs.unmount();
             }));
}

// Test fs.promises.lutimes on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.lutimes(path.join(mountPoint, 'src/hello.txt'), Date.now() / 1000, Date.now() / 1000)
    .then(common.mustCall(() => {
      myVfs.unmount();
    }));
}

// ==================== fdatasync ops ====================

// Test fs.fdatasyncSync on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.fdatasyncSync(fd);
  fs.closeSync(fd);
  myVfs.unmount();
}

// Test fs.fdatasync callback on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.fdatasync(fd, common.mustCall((err) => {
    assert.strictEqual(err, null);
    fs.closeSync(fd);
    myVfs.unmount();
  }));
}

// ==================== fsync ops ====================

// Test fs.fsyncSync on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.fsyncSync(fd);
  fs.closeSync(fd);
  myVfs.unmount();
}

// Test fs.fsync callback on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.fsync(fd, common.mustCall((err) => {
    assert.strictEqual(err, null);
    fs.closeSync(fd);
    myVfs.unmount();
  }));
}

// ==================== statfs ops ====================

// Test fs.statfsSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const stats = fs.statfsSync(path.join(mountPoint, 'src'));
  assert.strictEqual(stats.type, 0);
  assert.strictEqual(stats.bsize, 4096);
  assert.strictEqual(stats.bfree, 0);
  assert.strictEqual(stats.bavail, 0);
  assert.strictEqual(stats.files, 0);
  assert.strictEqual(stats.ffree, 0);
  myVfs.unmount();
}

// Test fs.statfs callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.statfs(path.join(mountPoint, 'src'), common.mustCall((err, stats) => {
    assert.strictEqual(err, null);
    assert.strictEqual(stats.type, 0);
    assert.strictEqual(stats.bsize, 4096);
    myVfs.unmount();
  }));
}

// Test fs.promises.statfs on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.statfs(path.join(mountPoint, 'src')).then(common.mustCall((stats) => {
    assert.strictEqual(stats.type, 0);
    assert.strictEqual(stats.bsize, 4096);
    assert.strictEqual(stats.bfree, 0);
    myVfs.unmount();
  }));
}

// ==================== readv ops ====================

// Test fs.readvSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  const buf1 = Buffer.alloc(5);
  const buf2 = Buffer.alloc(6);
  const bytesRead = fs.readvSync(fd, [buf1, buf2], 0);
  assert.strictEqual(bytesRead, 11);
  assert.strictEqual(buf1.toString(), 'hello');
  assert.strictEqual(buf2.toString(), ' world');
  fs.closeSync(fd);
  myVfs.unmount();
}

// Test fs.readv callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  const buf1 = Buffer.alloc(5);
  const buf2 = Buffer.alloc(6);
  fs.readv(fd, [buf1, buf2], 0, common.mustCall((err, bytesRead, buffers) => {
    assert.strictEqual(err, null);
    assert.strictEqual(bytesRead, 11);
    assert.strictEqual(buffers[0].toString(), 'hello');
    assert.strictEqual(buffers[1].toString(), ' world');
    fs.closeSync(fd);
    myVfs.unmount();
  }));
}

// ==================== writev ops ====================

// Test fs.writevSync on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/writev.txt'), 'w');
  const buf1 = Buffer.from('hello');
  const buf2 = Buffer.from(' writev');
  const bytesWritten = fs.writevSync(fd, [buf1, buf2], 0);
  assert.strictEqual(bytesWritten, 12);
  fs.closeSync(fd);
  const content = fs.readFileSync(path.join(mountPoint, 'src/writev.txt'), 'utf8');
  assert.strictEqual(content, 'hello writev');
  myVfs.unmount();
}

// Test fs.writev callback on VFS
{
  const { myVfs, mountPoint } = createMountedVfs();
  const fd = fs.openSync(path.join(mountPoint, 'src/writev-cb.txt'), 'w');
  const buf1 = Buffer.from('cb');
  const buf2 = Buffer.from(' writev');
  fs.writev(fd, [buf1, buf2], 0, common.mustCall((err, bytesWritten, buffers) => {
    assert.strictEqual(err, null);
    assert.strictEqual(bytesWritten, 9);
    assert.strictEqual(buffers.length, 2);
    fs.closeSync(fd);
    const content = fs.readFileSync(path.join(mountPoint, 'src/writev-cb.txt'), 'utf8');
    assert.strictEqual(content, 'cb writev');
    myVfs.unmount();
  }));
}

// ==================== promises.lchmod ====================

// Test fs.promises.lchmod on VFS (no-op)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.promises.lchmod(path.join(mountPoint, 'src/hello.txt'), 0o644).then(common.mustCall(() => {
    myVfs.unmount();
  }));
}
