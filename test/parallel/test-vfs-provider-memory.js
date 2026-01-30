'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

// Test copyFileSync
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/source.txt', 'original content');

  myVfs.copyFileSync('/source.txt', '/dest.txt');
  assert.strictEqual(myVfs.readFileSync('/dest.txt', 'utf8'), 'original content');

  // Source file should still exist
  assert.strictEqual(myVfs.existsSync('/source.txt'), true);

  // Test copying to nested path
  myVfs.mkdirSync('/nested/dir', { recursive: true });
  myVfs.copyFileSync('/source.txt', '/nested/dir/copy.txt');
  assert.strictEqual(myVfs.readFileSync('/nested/dir/copy.txt', 'utf8'), 'original content');

  // Test copying non-existent file
  assert.throws(() => {
    myVfs.copyFileSync('/nonexistent.txt', '/fail.txt');
  }, { code: 'ENOENT' });
}

// Test async copyFile
(async () => {
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/async-source.txt', 'async content');

  await myVfs.promises.copyFile('/async-source.txt', '/async-dest.txt');
  assert.strictEqual(myVfs.readFileSync('/async-dest.txt', 'utf8'), 'async content');

  // Test copying non-existent file
  await assert.rejects(
    myVfs.promises.copyFile('/nonexistent.txt', '/fail.txt'),
    { code: 'ENOENT' }
  );
})().then(common.mustCall());

// Test copyFileSync with mode argument
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/src-mode.txt', 'mode content');

  // copyFileSync also accepts a mode argument (ignored for VFS but tests the code path)
  myVfs.copyFileSync('/src-mode.txt', '/dest-mode.txt', 0);
  assert.strictEqual(myVfs.readFileSync('/dest-mode.txt', 'utf8'), 'mode content');
}

// Test appendFileSync
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/append.txt', 'hello');

  myVfs.appendFileSync('/append.txt', ' world');
  assert.strictEqual(myVfs.readFileSync('/append.txt', 'utf8'), 'hello world');

  // Append more
  myVfs.appendFileSync('/append.txt', '!');
  assert.strictEqual(myVfs.readFileSync('/append.txt', 'utf8'), 'hello world!');

  // Append to non-existent file creates it
  myVfs.appendFileSync('/newfile.txt', 'new content');
  assert.strictEqual(myVfs.readFileSync('/newfile.txt', 'utf8'), 'new content');
}

// Test async appendFile
(async () => {
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/async-append.txt', 'start');

  await myVfs.promises.appendFile('/async-append.txt', '-end');
  assert.strictEqual(myVfs.readFileSync('/async-append.txt', 'utf8'), 'start-end');
})().then(common.mustCall());

// Test appendFileSync with Buffer
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/buffer-append.txt', Buffer.from('start'));

  myVfs.appendFileSync('/buffer-append.txt', Buffer.from('-buffer'));
  assert.strictEqual(myVfs.readFileSync('/buffer-append.txt', 'utf8'), 'start-buffer');
}

// Test MemoryProvider readonly mode
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/file.txt', 'content');
  myVfs.mkdirSync('/dir', { recursive: true });

  // Set to readonly
  myVfs.provider.setReadOnly();
  assert.strictEqual(myVfs.provider.readonly, true);

  // Read operations should still work
  assert.strictEqual(myVfs.readFileSync('/file.txt', 'utf8'), 'content');
  assert.strictEqual(myVfs.existsSync('/file.txt'), true);
  assert.ok(myVfs.statSync('/file.txt'));

  // Write operations should throw EROFS
  assert.throws(() => {
    myVfs.writeFileSync('/file.txt', 'new content');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.writeFileSync('/new.txt', 'content');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.appendFileSync('/file.txt', 'more');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.mkdirSync('/newdir');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.unlinkSync('/file.txt');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.rmdirSync('/dir');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.renameSync('/file.txt', '/renamed.txt');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.copyFileSync('/file.txt', '/copy.txt');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.symlinkSync('/file.txt', '/link');
  }, { code: 'EROFS' });
}

// Test async operations on readonly VFS
(async () => {
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/readonly.txt', 'content');
  myVfs.provider.setReadOnly();

  await assert.rejects(
    myVfs.promises.writeFile('/readonly.txt', 'new'),
    { code: 'EROFS' }
  );

  await assert.rejects(
    myVfs.promises.appendFile('/readonly.txt', 'more'),
    { code: 'EROFS' }
  );

  await assert.rejects(
    myVfs.promises.mkdir('/newdir'),
    { code: 'EROFS' }
  );

  await assert.rejects(
    myVfs.promises.unlink('/readonly.txt'),
    { code: 'EROFS' }
  );

  await assert.rejects(
    myVfs.promises.copyFile('/readonly.txt', '/copy.txt'),
    { code: 'EROFS' }
  );
})().then(common.mustCall());

// Test accessSync
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/access-test.txt', 'content');

  // Should not throw for existing file
  myVfs.accessSync('/access-test.txt');

  // Should throw for non-existent file
  assert.throws(() => {
    myVfs.accessSync('/nonexistent.txt');
  }, { code: 'ENOENT' });
}
