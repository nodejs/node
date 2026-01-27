'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

// Test globSync with VFS mounted directory
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/src/lib/deep', { recursive: true });
  myVfs.mkdirSync('/src/empty', { recursive: true });
  myVfs.writeFileSync('/src/index.js', 'export default 1;');
  myVfs.writeFileSync('/src/utils.js', 'export const util = 1;');
  myVfs.writeFileSync('/src/lib/helper.js', 'export const helper = 1;');
  myVfs.writeFileSync('/src/lib/deep/nested.js', 'export const nested = 1;');
  myVfs.mount('/virtual');

  // Test simple glob pattern
  const jsFiles = fs.globSync('/virtual/src/*.js');
  assert.strictEqual(jsFiles.length, 2);
  assert.ok(jsFiles.includes('/virtual/src/index.js'));
  assert.ok(jsFiles.includes('/virtual/src/utils.js'));

  // Test recursive glob pattern
  const allJsFiles = fs.globSync('/virtual/src/**/*.js');
  assert.strictEqual(allJsFiles.length, 4);
  assert.ok(allJsFiles.includes('/virtual/src/index.js'));
  assert.ok(allJsFiles.includes('/virtual/src/utils.js'));
  assert.ok(allJsFiles.includes('/virtual/src/lib/helper.js'));
  assert.ok(allJsFiles.includes('/virtual/src/lib/deep/nested.js'));

  // Test glob with directory matching
  const dirs = fs.globSync('/virtual/src/*/', { withFileTypes: false });
  // Glob returns paths ending with / for directories
  assert.ok(dirs.some((d) => d.includes('lib')));
  assert.ok(dirs.some((d) => d.includes('empty')));

  myVfs.unmount();
}

// Test async glob (callback API) with VFS mounted directory
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/async-src/lib', { recursive: true });
  myVfs.writeFileSync('/async-src/index.js', 'export default 1;');
  myVfs.writeFileSync('/async-src/utils.js', 'export const util = 1;');
  myVfs.writeFileSync('/async-src/lib/helper.js', 'export const helper = 1;');
  myVfs.mount('/async-virtual');

  fs.glob('/async-virtual/async-src/*.js', common.mustCall((err, files) => {
    assert.strictEqual(err, null);
    assert.strictEqual(files.length, 2);
    assert.ok(files.includes('/async-virtual/async-src/index.js'));
    assert.ok(files.includes('/async-virtual/async-src/utils.js'));

    // Test recursive pattern with callback
    fs.glob('/async-virtual/async-src/**/*.js', common.mustCall((err, allFiles) => {
      assert.strictEqual(err, null);
      assert.strictEqual(allFiles.length, 3);
      assert.ok(allFiles.includes('/async-virtual/async-src/index.js'));
      assert.ok(allFiles.includes('/async-virtual/async-src/utils.js'));
      assert.ok(allFiles.includes('/async-virtual/async-src/lib/helper.js'));

      myVfs.unmount();
    }));
  }));
}

// Test async glob (promise API) with VFS
(async () => {
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/promise-src', { recursive: true });
  myVfs.writeFileSync('/promise-src/a.ts', 'const a = 1;');
  myVfs.writeFileSync('/promise-src/b.ts', 'const b = 2;');
  myVfs.writeFileSync('/promise-src/c.js', 'const c = 3;');
  myVfs.mount('/promise-virtual');

  const { glob } = require('node:fs/promises');

  // Glob returns an async iterator, need to collect results
  const tsFiles = [];
  for await (const file of glob('/promise-virtual/promise-src/*.ts')) {
    tsFiles.push(file);
  }
  assert.strictEqual(tsFiles.length, 2);
  assert.ok(tsFiles.includes('/promise-virtual/promise-src/a.ts'));
  assert.ok(tsFiles.includes('/promise-virtual/promise-src/b.ts'));

  // Test multiple patterns
  const allFiles = [];
  for await (const file of glob(['/promise-virtual/promise-src/*.ts', '/promise-virtual/promise-src/*.js'])) {
    allFiles.push(file);
  }
  assert.strictEqual(allFiles.length, 3);

  myVfs.unmount();
})().then(common.mustCall());

// Test glob with withFileTypes option
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/typed/subdir', { recursive: true });
  myVfs.writeFileSync('/typed/file.txt', 'text');
  myVfs.writeFileSync('/typed/subdir/nested.txt', 'nested');
  myVfs.mount('/typedvfs');

  const entries = fs.globSync('/typedvfs/typed/*', { withFileTypes: true });
  assert.strictEqual(entries.length, 2);

  const fileEntry = entries.find((e) => e.name === 'file.txt');
  assert.ok(fileEntry);
  assert.strictEqual(fileEntry.isFile(), true);
  assert.strictEqual(fileEntry.isDirectory(), false);

  const dirEntry = entries.find((e) => e.name === 'subdir');
  assert.ok(dirEntry);
  assert.strictEqual(dirEntry.isFile(), false);
  assert.strictEqual(dirEntry.isDirectory(), true);

  myVfs.unmount();
}

// Test glob with multiple patterns
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/multi', { recursive: true });
  myVfs.writeFileSync('/multi/a.js', 'a');
  myVfs.writeFileSync('/multi/b.ts', 'b');
  myVfs.writeFileSync('/multi/c.md', 'c');
  myVfs.mount('/multipat');

  const files = fs.globSync(['/multipat/multi/*.js', '/multipat/multi/*.ts']);
  assert.strictEqual(files.length, 2);
  assert.ok(files.includes('/multipat/multi/a.js'));
  assert.ok(files.includes('/multipat/multi/b.ts'));

  myVfs.unmount();
}

// Test that unmounting stops glob from finding VFS files
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/unmount-test', { recursive: true });
  myVfs.writeFileSync('/unmount-test/file.js', 'content');
  myVfs.mount('/unmount-glob');

  let files = fs.globSync('/unmount-glob/unmount-test/*.js');
  assert.strictEqual(files.length, 1);

  myVfs.unmount();

  files = fs.globSync('/unmount-glob/unmount-test/*.js');
  assert.strictEqual(files.length, 0);
}

// Test glob pattern that doesn't match anything
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/nomatch', { recursive: true });
  myVfs.writeFileSync('/nomatch/file.txt', 'content');
  myVfs.mount('/nomatchvfs');

  const files = fs.globSync('/nomatchvfs/nomatch/*.nonexistent');
  assert.strictEqual(files.length, 0);

  myVfs.unmount();
}

// Test cwd option with VFS (relative patterns)
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/cwd-test', { recursive: true });
  myVfs.writeFileSync('/cwd-test/a.js', 'a');
  myVfs.writeFileSync('/cwd-test/b.js', 'b');
  myVfs.mount('/cwdvfs');

  const files = fs.globSync('*.js', { cwd: '/cwdvfs/cwd-test' });
  assert.strictEqual(files.length, 2);
  assert.ok(files.includes('a.js'));
  assert.ok(files.includes('b.js'));

  myVfs.unmount();
}
