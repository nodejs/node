import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import { resolve } from 'node:path';
import { mkdir, writeFile, symlink, glob as asyncGlob } from 'node:fs/promises';
import { globSync } from 'node:fs';
import { test } from 'node:test';
import assert from 'node:assert';

tmpdir.refresh();

const fixtureDir = tmpdir.resolve('glob-symlink-test');

async function setup() {
  await mkdir(fixtureDir, { recursive: true });

  // Create a real directory with files
  const realDir = resolve(fixtureDir, 'real-dir');
  await mkdir(realDir, { recursive: true });
  await writeFile(resolve(realDir, 'file.txt'), 'hello NodeJS Team');
  await writeFile(resolve(realDir, 'test.js'), 'console.log("test")');

  // Create a subdirectory in the real directory
  const subDir = resolve(realDir, 'subdir');
  await mkdir(subDir, { recursive: true });
  await writeFile(resolve(subDir, 'nested.txt'), 'nested file');

  // Create a symlink to the real directory
  const symlinkDir = resolve(fixtureDir, 'symlinked-dir');
  await symlink(realDir, symlinkDir, 'dir');

  // Create another regular directory for comparison
  const regularDir = resolve(fixtureDir, 'regular-dir');
  await mkdir(regularDir, { recursive: true });
  await writeFile(resolve(regularDir, 'regular.txt'), 'regular file');
}

await setup();

test('glob does not follow symlinks by default', async () => {
  if (common.isWindows) {
    // Skip on Windows as symlinks require special permissions
    return;
  }

  const results = [];
  for await (const file of asyncGlob('**/*.txt', { cwd: fixtureDir })) {
    results.push(file);
  }
  const sortedResults = results.sort();

  // Should not include files from symlinked-dir since symlinks are not
  // followed by default
  assert.ok(sortedResults.includes('real-dir/file.txt'),
            'Should include real-dir/file.txt');
  assert.ok(sortedResults.includes('real-dir/subdir/nested.txt'),
            'Should include real-dir/subdir/nested.txt');
  assert.ok(sortedResults.includes('regular-dir/regular.txt'),
            'Should include regular-dir/regular.txt');
  assert.ok(!sortedResults.includes('symlinked-dir/file.txt'),
            'Should not include symlinked-dir/file.txt by default');
  assert.ok(!sortedResults.includes('symlinked-dir/subdir/nested.txt'),
            'Should not include symlinked-dir/subdir/nested.txt by default');
});

test('glob follows symlinks when followSymlinks is true', async () => {
  if (common.isWindows) {
    // Skip on Windows as symlinks require special permissions
    return;
  }

  const results = [];
  for await (const file of asyncGlob('**/*.txt',
                                     { cwd: fixtureDir, followSymlinks: true })) {
    results.push(file);
  }
  const sortedResults = results.sort();

  // Should include files from symlinked-dir when followSymlinks is enabled
  assert.ok(sortedResults.includes('real-dir/file.txt'),
            'Should include real-dir/file.txt');
  assert.ok(sortedResults.includes('real-dir/subdir/nested.txt'),
            'Should include real-dir/subdir/nested.txt');
  assert.ok(sortedResults.includes('regular-dir/regular.txt'),
            'Should include regular-dir/regular.txt');
  assert.ok(sortedResults.includes('symlinked-dir/file.txt'),
            'Should include symlinked-dir/file.txt when followSymlinks is true');
  assert.ok(sortedResults.includes('symlinked-dir/subdir/nested.txt'),
            'Should include files in symlinked-dir/subdir when followSymlinks is true');
});

test('globSync does not follow symlinks by default', () => {
  if (common.isWindows) {
    // Skip on Windows as symlinks require special permissions
    return;
  }

  const results = globSync('**/*.txt', { cwd: fixtureDir });
  const sortedResults = results.sort();

  // Should not include files from symlinked-dir since symlinks are not
  // followed by default
  assert.ok(sortedResults.includes('real-dir/file.txt'),
            'Should include real-dir/file.txt');
  assert.ok(sortedResults.includes('real-dir/subdir/nested.txt'),
            'Should include real-dir/subdir/nested.txt');
  assert.ok(sortedResults.includes('regular-dir/regular.txt'),
            'Should include regular-dir/regular.txt');
  assert.ok(!sortedResults.includes('symlinked-dir/file.txt'),
            'Should not include symlinked-dir/file.txt by default');
  assert.ok(!sortedResults.includes('symlinked-dir/subdir/nested.txt'),
            'Should not include symlinked-dir/subdir/nested.txt by default');
});

test('globSync follows symlinks when followSymlinks is true', () => {
  if (common.isWindows) {
    // Skip on Windows as symlinks require special permissions
    return;
  }

  const results = globSync('**/*.txt',
                           { cwd: fixtureDir, followSymlinks: true });
  const sortedResults = results.sort();

  // Should include files from symlinked-dir when followSymlinks is enabled
  assert.ok(sortedResults.includes('real-dir/file.txt'),
            'Should include real-dir/file.txt');
  assert.ok(sortedResults.includes('real-dir/subdir/nested.txt'),
            'Should include real-dir/subdir/nested.txt');
  assert.ok(sortedResults.includes('regular-dir/regular.txt'),
            'Should include regular-dir/regular.txt');
  assert.ok(sortedResults.includes('symlinked-dir/file.txt'),
            'Should include symlinked-dir/file.txt when followSymlinks is true');
  assert.ok(sortedResults.includes('symlinked-dir/subdir/nested.txt'),
            'Should include files in symlinked-dir/subdir when followSymlinks is true');
});

test('glob with ** pattern follows symlinks when followSymlinks is true', async () => {
  if (common.isWindows) {
    // Skip on Windows as symlinks require special permissions
    return;
  }

  const results = [];
  for await (const file of asyncGlob('**', { cwd: fixtureDir, followSymlinks: true })) {
    results.push(file);
  }
  const sortedResults = results.sort();

  // Should include the symlinked directory contents
  assert.ok(sortedResults.includes('symlinked-dir'),
            'Should include symlinked-dir');
  assert.ok(sortedResults.includes('symlinked-dir/file.txt'),
            'Should include symlinked-dir/file.txt');
  assert.ok(sortedResults.includes('symlinked-dir/test.js'),
            'Should include symlinked-dir/test.js');
  assert.ok(sortedResults.includes('symlinked-dir/subdir'),
            'Should include symlinked-dir/subdir');
  assert.ok(sortedResults.includes('symlinked-dir/subdir/nested.txt'),
            'Should include symlinked-dir/subdir/nested.txt');
});

test('glob with followSymlinks=false explicitly set', async () => {
  if (common.isWindows) {
    // Skip on Windows as symlinks require special permissions
    return;
  }

  const results = [];
  for await (const file of asyncGlob('**/*.txt', { cwd: fixtureDir, followSymlinks: false })) {
    results.push(file);
  }
  const sortedResults = results.sort();

  // Should not include files from symlinked-dir
  assert.ok(sortedResults.includes('real-dir/file.txt'),
            'Should include real-dir/file.txt');
  assert.ok(!sortedResults.includes('symlinked-dir/file.txt'),
            'Should not include symlinked-dir/file.txt when followSymlinks is false');
});

test('glob with withFileTypes and followSymlinks', async () => {
  if (common.isWindows) {
    // Skip on Windows as symlinks require special permissions
    return;
  }

  const results = [];
  for await (const entry of asyncGlob('**/*.txt', { cwd: fixtureDir, withFileTypes: true, followSymlinks: true })) {
    results.push(entry);
  }

  const names = results.map((r) => r.name).sort();

  // Should include files from symlinked-dir
  assert.ok(names.includes('file.txt'),
            'Should include file.txt');
  assert.ok(names.includes('nested.txt'),
            'Should include nested.txt');
  assert.ok(names.includes('regular.txt'),
            'Should include regular.txt');

  // Verify all results are file entries (not symlinks anymore, they should be resolved as files)
  results.forEach((entry) => {
    assert.ok(entry.isFile() || entry.isDirectory(), `Entry ${entry.name} should be a file or directory`);
  });
});
