'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');

// Test that VirtualFileSystem can be created via fs.createVirtual()
{
  const myVfs = fs.createVirtual();
  assert.ok(myVfs);
  assert.strictEqual(typeof myVfs.addFile, 'function');
  assert.strictEqual(myVfs.isMounted, false);
  assert.strictEqual(myVfs.isOverlay, false);
  assert.strictEqual(myVfs.fallthrough, true);
}

// Test adding and reading a static file
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/test/file.txt', 'hello world');

  assert.strictEqual(myVfs.existsSync('/test/file.txt'), true);
  assert.strictEqual(myVfs.existsSync('/test'), true);
  assert.strictEqual(myVfs.existsSync('/nonexistent'), false);

  const content = myVfs.readFileSync('/test/file.txt');
  assert.ok(Buffer.isBuffer(content));
  assert.strictEqual(content.toString(), 'hello world');

  // Read with encoding
  const textContent = myVfs.readFileSync('/test/file.txt', 'utf8');
  assert.strictEqual(textContent, 'hello world');
}

// Test statSync
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/test/file.txt', 'content');
  myVfs.addDirectory('/test/dir');

  const fileStat = myVfs.statSync('/test/file.txt');
  assert.strictEqual(fileStat.isFile(), true);
  assert.strictEqual(fileStat.isDirectory(), false);
  assert.strictEqual(fileStat.size, 7); // 'content'.length

  const dirStat = myVfs.statSync('/test/dir');
  assert.strictEqual(dirStat.isFile(), false);
  assert.strictEqual(dirStat.isDirectory(), true);

  // Test ENOENT
  assert.throws(() => {
    myVfs.statSync('/nonexistent');
  }, { code: 'ENOENT' });
}

// Test readdirSync
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/dir/a.txt', 'a');
  myVfs.addFile('/dir/b.txt', 'b');
  myVfs.addDirectory('/dir/subdir');

  const entries = myVfs.readdirSync('/dir');
  assert.deepStrictEqual(entries.sort(), ['a.txt', 'b.txt', 'subdir']);

  // Test with file types
  const dirents = myVfs.readdirSync('/dir', { withFileTypes: true });
  assert.strictEqual(dirents.length, 3);

  const fileEntry = dirents.find((d) => d.name === 'a.txt');
  assert.ok(fileEntry);
  assert.strictEqual(fileEntry.isFile(), true);
  assert.strictEqual(fileEntry.isDirectory(), false);

  const dirEntry = dirents.find((d) => d.name === 'subdir');
  assert.ok(dirEntry);
  assert.strictEqual(dirEntry.isFile(), false);
  assert.strictEqual(dirEntry.isDirectory(), true);

  // Test ENOTDIR
  assert.throws(() => {
    myVfs.readdirSync('/dir/a.txt');
  }, { code: 'ENOTDIR' });

  // Test ENOENT
  assert.throws(() => {
    myVfs.readdirSync('/nonexistent');
  }, { code: 'ENOENT' });
}

// Test dynamic file content
{
  const myVfs = fs.createVirtual();
  let counter = 0;
  myVfs.addFile('/dynamic.txt', () => {
    counter++;
    return `count: ${counter}`;
  });

  assert.strictEqual(myVfs.readFileSync('/dynamic.txt', 'utf8'), 'count: 1');
  assert.strictEqual(myVfs.readFileSync('/dynamic.txt', 'utf8'), 'count: 2');
  assert.strictEqual(counter, 2);
}

// Test dynamic directory
{
  const myVfs = fs.createVirtual();
  let populated = false;

  myVfs.addDirectory('/dynamic', (dir) => {
    populated = true;
    dir.addFile('generated.txt', 'generated content');
    dir.addDirectory('subdir', (subdir) => {
      subdir.addFile('nested.txt', 'nested content');
    });
  });

  // Directory should not be populated until accessed
  assert.strictEqual(populated, false);

  // Access the directory
  const entries = myVfs.readdirSync('/dynamic');
  assert.strictEqual(populated, true);
  assert.deepStrictEqual(entries.sort(), ['generated.txt', 'subdir']);

  // Read generated file
  assert.strictEqual(myVfs.readFileSync('/dynamic/generated.txt', 'utf8'), 'generated content');

  // Access nested dynamic directory
  assert.strictEqual(myVfs.readFileSync('/dynamic/subdir/nested.txt', 'utf8'), 'nested content');
}

// Test removing entries
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/test/file.txt', 'content');

  assert.strictEqual(myVfs.has('/test/file.txt'), true);
  assert.strictEqual(myVfs.remove('/test/file.txt'), true);
  assert.strictEqual(myVfs.has('/test/file.txt'), false);

  // Cannot remove non-existent entry
  assert.strictEqual(myVfs.remove('/nonexistent'), false);
}

// Test mount mode
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/data/file.txt', 'mounted content');

  assert.strictEqual(myVfs.isMounted, false);
  myVfs.mount('/app/virtual');
  assert.strictEqual(myVfs.isMounted, true);
  assert.strictEqual(myVfs.mountPoint, '/app/virtual');

  // With mount, shouldHandle should work
  assert.strictEqual(myVfs.shouldHandle('/app/virtual/data/file.txt'), true);
  assert.strictEqual(myVfs.shouldHandle('/other/path'), false);

  myVfs.unmount();
  assert.strictEqual(myVfs.isMounted, false);
}

// Test overlay mode
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/overlay/file.txt', 'overlay content');

  assert.strictEqual(myVfs.isOverlay, false);
  myVfs.overlay();
  assert.strictEqual(myVfs.isOverlay, true);

  // shouldHandle based on existence
  assert.strictEqual(myVfs.shouldHandle('/overlay/file.txt'), true);
  assert.strictEqual(myVfs.shouldHandle('/nonexistent'), false);

  myVfs.unmount();
  assert.strictEqual(myVfs.isOverlay, false);
}

// Test internalModuleStat (used by Module._stat)
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/module.js', 'module.exports = {}');
  myVfs.addDirectory('/dir');

  assert.strictEqual(myVfs.internalModuleStat('/module.js'), 0); // file
  assert.strictEqual(myVfs.internalModuleStat('/dir'), 1); // directory
  assert.strictEqual(myVfs.internalModuleStat('/nonexistent'), -2); // ENOENT
}

// Test reading directory as file throws EISDIR
{
  const myVfs = fs.createVirtual();
  myVfs.addDirectory('/mydir');

  assert.throws(() => {
    myVfs.readFileSync('/mydir');
  }, { code: 'EISDIR' });
}

// Test realpathSync
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/test/file.txt', 'content');

  const realpath = myVfs.realpathSync('/test/file.txt');
  assert.strictEqual(realpath, '/test/file.txt');

  // Non-existent path throws
  assert.throws(() => {
    myVfs.realpathSync('/nonexistent');
  }, { code: 'ENOENT' });
}
