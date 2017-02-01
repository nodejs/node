'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const fn = path.join(common.fixturesDir, 'non-existent');
const existingFile = path.join(common.fixturesDir, 'exit.js');
const existingFile2 = path.join(common.fixturesDir, 'create-file.js');
const existingDir = path.join(common.fixturesDir, 'empty');
const existingDir2 = path.join(common.fixturesDir, 'keys');

// Test all operations failing with ENOENT errors
function testEnoentError(file, endMessage, syscal, err) {
  const sufix = (endMessage) ? endMessage : '';

  assert(err instanceof Error);
  assert.strictEqual(fn, err.path);
  assert.strictEqual(
    err.message,
    `ENOENT: no such file or directory, ${syscal} '${file}'${sufix}`
  );

  return true;
}

// Test all operations failing with EEXIST errors
function testEexistError(source, dest, syscal, err) {
  const sufix = (dest) ? ` -> '${dest}'` : '';

  assert(err instanceof Error);
  assert.strictEqual(source, err.path);
  assert.strictEqual(
    err.message,
    `EEXIST: file already exists, ${syscal} '${source}'${sufix}`
  );

  return true;
}

// Test all operations failing with ENOTEMPTY errors
function testEnoemptyError(source, dest, err) {
  assert(err instanceof Error);
  assert.strictEqual(source, err.path);
  assert.strictEqual(
    err.message,
    `ENOTEMPTY: directory not empty, rename '${source}' ` +
    `-> '${dest}'`
  );

  return true;
}

// Test all operations failing with ENOTDIR errors
function testEnotdirError(dir, err) {
  assert(err instanceof Error);
  assert.strictEqual(dir, err.path);
  assert.strictEqual(
    err.message,
    `ENOTDIR: not a directory, rmdir '${dir}'`
  );

  return true;
}

// Generating ENOENTS errors
fs.stat(fn, (err) => testEnoentError(fn, '', 'stat', err));
fs.lstat(fn, (err) => testEnoentError(fn, '', 'lstat', err));
fs.readlink(fn, (err) => testEnoentError(fn, '', 'readlink', err));
fs.link(fn, 'foo', (err) => testEnoentError(fn, ' -> \'foo\'', 'link', err));
fs.unlink(fn, (err) => testEnoentError(fn, '', 'unlink', err));
fs.rmdir(fn, (err) => testEnoentError(fn, '', 'rmdir', err));
fs.chmod(fn, 0o666, (err) => testEnoentError(fn, '', 'chmod', err));
fs.open(fn, 'r', 0o666, (err) => testEnoentError(fn, '', 'open', err));
fs.readFile(fn, (err) => testEnoentError(fn, '', 'open', err));
fs.readdir(fn, (err) => testEnoentError(fn, '', 'scandir', err));

fs.rename(fn, 'foo', (err) => {
  testEnoentError(fn, ' -> \'foo\'', 'rename', err);
});

assert.throws(() => {
  fs.statSync(fn);
}, (err) => testEnoentError(fn, '', 'stat', err));

assert.throws(() => {
  fs.lstatSync(fn);
}, (err) => testEnoentError(fn, '', 'lstat', err));

assert.throws(() => {
  fs.readlinkSync(fn);
}, (err) => testEnoentError(fn, '', 'readlink', err));

assert.throws(() => {
  fs.linkSync(fn, 'foo');
}, (err) => testEnoentError(fn, ' -> \'foo\'', 'link', err));

assert.throws(() => {
  fs.unlinkSync(fn);
}, (err) => testEnoentError(fn, '', 'unlink', err));

assert.throws(() => {
  fs.rmdirSync(fn);
}, (err) => testEnoentError(fn, '', 'rmdir', err));

assert.throws(() => {
  fs.chmodSync(fn, 0o666);
}, (err) => testEnoentError(fn, '', 'chmod', err));

assert.throws(() => {
  fs.openSync(fn, 'r');
}, (err) => testEnoentError(fn, '', 'open', err));

assert.throws(() => {
  fs.readFileSync(fn);
}, (err) => testEnoentError(fn, '', 'open', err));

assert.throws(() => {
  fs.readdirSync(fn);
}, (err) => testEnoentError(fn, '', 'scandir', err));

assert.throws(() => {
  fs.renameSync(fn, 'foo');
}, (err) => testEnoentError(fn, ' -> \'foo\'', 'rename', err));

// Generating EEXIST errors
fs.link(existingFile, existingFile2, (err) => {
  testEexistError(existingFile, existingFile2, 'link', err);
});

fs.symlink(existingFile, existingFile2, (err) => {
  testEexistError(existingFile, existingFile2, 'symlink', err);
});

fs.mkdir(existingFile, 0o666, (err) => {
  testEexistError(existingFile, null, 'mkdir', err);
});

assert.throws(() => {
  fs.linkSync(existingFile, existingFile2);
}, (err) => testEexistError(existingFile, existingFile2, 'link', err));

assert.throws(() => {
  fs.symlinkSync(existingFile, existingFile2);
}, (err) => testEexistError(existingFile, existingFile2, 'symlink', err));

assert.throws(() => {
  fs.mkdirSync(existingFile, 0o666);
}, (err) => testEexistError(existingFile, null, 'mkdir', err));

// Generating ENOTEMPTY errors
fs.rename(existingDir, existingDir2, (err) => {
  testEnoemptyError(existingDir, existingDir2, err);
});

assert.throws(() => {
  fs.renameSync(existingDir, existingDir2);
}, (err) => testEnoemptyError(existingDir, existingDir2, err));

// Generating ENOTDIR errors
fs.rmdir(existingFile, (err) => testEnotdirError(existingFile, err));

assert.throws(() => {
  fs.rmdirSync(existingFile);
}, (err) => testEnotdirError(existingFile, err));
