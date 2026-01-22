'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

// Test openSync and closeSync
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/file.txt', 'hello world');

  const fd = myVfs.openSync('/file.txt');
  assert.ok(fd >= 10000, 'VFS fd should be >= 10000');
  myVfs.closeSync(fd);
}

// Test openSync with non-existent file
{
  const myVfs = fs.createVirtual();

  assert.throws(() => {
    myVfs.openSync('/nonexistent.txt');
  }, { code: 'ENOENT' });
}

// Test openSync with directory
{
  const myVfs = fs.createVirtual();
  myVfs.addDirectory('/mydir');

  assert.throws(() => {
    myVfs.openSync('/mydir');
  }, { code: 'EISDIR' });
}

// Test closeSync with invalid fd
{
  const myVfs = fs.createVirtual();

  assert.throws(() => {
    myVfs.closeSync(12345);
  }, { code: 'EBADF' });
}

// Test readSync
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/file.txt', 'hello world');

  const fd = myVfs.openSync('/file.txt');
  const buffer = Buffer.alloc(5);

  const bytesRead = myVfs.readSync(fd, buffer, 0, 5, 0);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buffer.toString(), 'hello');

  myVfs.closeSync(fd);
}

// Test readSync with position tracking
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/file.txt', 'hello world');

  const fd = myVfs.openSync('/file.txt');
  const buffer1 = Buffer.alloc(5);
  const buffer2 = Buffer.alloc(6);

  // Read first 5 bytes
  let bytesRead = myVfs.readSync(fd, buffer1, 0, 5, null);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buffer1.toString(), 'hello');

  // Continue reading (position should advance)
  bytesRead = myVfs.readSync(fd, buffer2, 0, 6, null);
  assert.strictEqual(bytesRead, 6);
  assert.strictEqual(buffer2.toString(), ' world');

  myVfs.closeSync(fd);
}

// Test readSync with explicit position
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/file.txt', 'hello world');

  const fd = myVfs.openSync('/file.txt');
  const buffer = Buffer.alloc(5);

  // Read from position 6 (start of "world")
  const bytesRead = myVfs.readSync(fd, buffer, 0, 5, 6);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buffer.toString(), 'world');

  myVfs.closeSync(fd);
}

// Test readSync at end of file
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/file.txt', 'short');

  const fd = myVfs.openSync('/file.txt');
  const buffer = Buffer.alloc(10);

  // Read from position beyond file
  const bytesRead = myVfs.readSync(fd, buffer, 0, 10, 100);
  assert.strictEqual(bytesRead, 0);

  myVfs.closeSync(fd);
}

// Test readSync with invalid fd
{
  const myVfs = fs.createVirtual();
  const buffer = Buffer.alloc(10);

  assert.throws(() => {
    myVfs.readSync(99999, buffer, 0, 10, 0);
  }, { code: 'EBADF' });
}

// Test fstatSync
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/file.txt', 'hello world');

  const fd = myVfs.openSync('/file.txt');
  const stats = myVfs.fstatSync(fd);

  assert.strictEqual(stats.isFile(), true);
  assert.strictEqual(stats.isDirectory(), false);
  assert.strictEqual(stats.size, 11);

  myVfs.closeSync(fd);
}

// Test fstatSync with invalid fd
{
  const myVfs = fs.createVirtual();

  assert.throws(() => {
    myVfs.fstatSync(99999);
  }, { code: 'EBADF' });
}

// Test async open and close
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/async-file.txt', 'async content');

  myVfs.open('/async-file.txt', common.mustCall((err, fd) => {
    assert.strictEqual(err, null);
    assert.ok(fd >= 10000);

    myVfs.close(fd, common.mustCall((err) => {
      assert.strictEqual(err, null);
    }));
  }));
}

// Test async open with error
{
  const myVfs = fs.createVirtual();

  myVfs.open('/nonexistent.txt', common.mustCall((err, fd) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(fd, undefined);
  }));
}

// Test async close with invalid fd
{
  const myVfs = fs.createVirtual();

  myVfs.close(99999, common.mustCall((err) => {
    assert.strictEqual(err.code, 'EBADF');
  }));
}

// Test async read
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/read-test.txt', 'read content');

  myVfs.open('/read-test.txt', common.mustCall((err, fd) => {
    assert.strictEqual(err, null);

    const buffer = Buffer.alloc(4);
    myVfs.read(fd, buffer, 0, 4, 0, common.mustCall((err, bytesRead, buf) => {
      assert.strictEqual(err, null);
      assert.strictEqual(bytesRead, 4);
      assert.strictEqual(buf, buffer);
      assert.strictEqual(buffer.toString(), 'read');

      myVfs.close(fd, common.mustCall());
    }));
  }));
}

// Test async read with position tracking
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/track-test.txt', 'ABCDEFGHIJ');

  myVfs.open('/track-test.txt', common.mustCall((err, fd) => {
    assert.strictEqual(err, null);

    const buffer1 = Buffer.alloc(3);
    const buffer2 = Buffer.alloc(3);

    myVfs.read(fd, buffer1, 0, 3, null, common.mustCall((err, bytesRead, buf) => {
      assert.strictEqual(err, null);
      assert.strictEqual(bytesRead, 3);
      assert.strictEqual(buffer1.toString(), 'ABC');

      // Continue reading without explicit position
      myVfs.read(fd, buffer2, 0, 3, null, common.mustCall((err, bytesRead, buf) => {
        assert.strictEqual(err, null);
        assert.strictEqual(bytesRead, 3);
        assert.strictEqual(buffer2.toString(), 'DEF');

        myVfs.close(fd, common.mustCall());
      }));
    }));
  }));
}

// Test async read with invalid fd
{
  const myVfs = fs.createVirtual();
  const buffer = Buffer.alloc(10);

  myVfs.read(99999, buffer, 0, 10, 0, common.mustCall((err, bytesRead, buf) => {
    assert.strictEqual(err.code, 'EBADF');
  }));
}

// Test async fstat
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/fstat-test.txt', '12345');

  myVfs.open('/fstat-test.txt', common.mustCall((err, fd) => {
    assert.strictEqual(err, null);

    myVfs.fstat(fd, common.mustCall((err, stats) => {
      assert.strictEqual(err, null);
      assert.strictEqual(stats.isFile(), true);
      assert.strictEqual(stats.size, 5);

      myVfs.close(fd, common.mustCall());
    }));
  }));
}

// Test async fstat with invalid fd
{
  const myVfs = fs.createVirtual();

  myVfs.fstat(99999, common.mustCall((err, stats) => {
    assert.strictEqual(err.code, 'EBADF');
  }));
}

// Test that separate VFS instances have separate fd spaces
{
  const vfs1 = fs.createVirtual();
  const vfs2 = fs.createVirtual();

  vfs1.addFile('/file1.txt', 'content1');
  vfs2.addFile('/file2.txt', 'content2');

  const fd1 = vfs1.openSync('/file1.txt');
  const fd2 = vfs2.openSync('/file2.txt');

  // Both should get valid fds
  assert.ok(fd1 >= 10000);
  assert.ok(fd2 >= 10000);

  // Read from fd1 using vfs1
  const buf1 = Buffer.alloc(8);
  const read1 = vfs1.readSync(fd1, buf1, 0, 8, 0);
  assert.strictEqual(read1, 8);
  assert.strictEqual(buf1.toString(), 'content1');

  // Read from fd2 using vfs2
  const buf2 = Buffer.alloc(8);
  const read2 = vfs2.readSync(fd2, buf2, 0, 8, 0);
  assert.strictEqual(read2, 8);
  assert.strictEqual(buf2.toString(), 'content2');

  vfs1.closeSync(fd1);
  vfs2.closeSync(fd2);
}

// Test multiple opens of same file
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/multi.txt', 'multi content');

  const fd1 = myVfs.openSync('/multi.txt');
  const fd2 = myVfs.openSync('/multi.txt');

  assert.notStrictEqual(fd1, fd2);

  const buf1 = Buffer.alloc(5);
  const buf2 = Buffer.alloc(5);

  myVfs.readSync(fd1, buf1, 0, 5, 0);
  myVfs.readSync(fd2, buf2, 0, 5, 0);

  assert.strictEqual(buf1.toString(), 'multi');
  assert.strictEqual(buf2.toString(), 'multi');

  myVfs.closeSync(fd1);
  myVfs.closeSync(fd2);
}
