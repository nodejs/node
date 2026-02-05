'use strict';

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test MemoryProvider can be instantiated directly
{
  const provider = new vfs.MemoryProvider();
  assert.strictEqual(provider.readonly, false);
  assert.strictEqual(provider.supportsSymlinks, true);
}

// Test MemoryProvider readonly mode
{
  const myVfs = vfs.create();
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

// Test async operations on readonly MemoryProvider
(async () => {
  const myVfs = vfs.create();
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
