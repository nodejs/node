'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

common.refreshTmpDir();
const fds = [];

const filename = path.resolve(common.tmpDir, 'truncate-file.txt');
fs.writeFileSync(filename, 'hello world', 'utf8');
const fd = fs.openSync(filename, 'r+');
fds.push(fd);
fs.truncate(fd, 5, common.mustCall(function(err) {
  assert.ifError(err);
  assert.equal(fs.readFileSync(filename, 'utf8'), 'hello');
}));

{
  // test partial truncation of a file
  const fileName = path.resolve(common.tmpDir, 'truncate-file-1.txt');
  console.log(fileName);
  fs.writeFileSync(fileName, 'hello world', 'utf8');
  const fd = fs.openSync(fileName, 'r+');
  fds.push(fd);

  fs.truncate(fd, 5, common.mustCall(function(err) {
    assert.ifError(err);
    assert.strictEqual(fs.readFileSync(fileName, 'utf8'), 'hello');
  }));
}

{
  // make sure numbers as strings are not treated as fds with sync version
  const fileName = path.resolve(common.tmpDir, 'truncate-file-2.txt');
  console.log(fileName);
  fs.writeFileSync(fileName, 'One');
  const fd = fs.openSync(fileName, 'r');
  fds.push(fd);

  const fdFileName = path.resolve(common.tmpDir, '' + fd);
  fs.writeFileSync(fdFileName, 'Two');
  assert.strictEqual(fs.readFileSync(fileName).toString(), 'One');
  assert.strictEqual(fs.readFileSync(fdFileName).toString(), 'Two');

  fs.truncateSync(fdFileName);
  assert.strictEqual(fs.readFileSync(fileName).toString(), 'One');
  assert.strictEqual(fs.readFileSync(fdFileName).toString(), '');
}

{
  // make sure numbers as strings are not treated as fds with async version
  const fileName = path.resolve(common.tmpDir, 'truncate-file-3.txt');
  console.log(fileName);
  fs.writeFileSync(fileName, 'One');
  const fd = fs.openSync(fileName, 'r');
  fds.push(fd);

  const fdFileName = path.resolve(common.tmpDir, '' + fd);
  fs.writeFileSync(fdFileName, 'Two');
  assert.strictEqual(fs.readFileSync(fileName).toString(), 'One');
  assert.strictEqual(fs.readFileSync(fdFileName).toString(), 'Two');

  fs.truncate(fdFileName, common.mustCall(function(err) {
    assert.ifError(err);
    assert.strictEqual(fs.readFileSync(fileName).toString(), 'One');
    assert.strictEqual(fs.readFileSync(fdFileName).toString(), '');
  }));
}

process.on('exit', () => fds.forEach((fd) => fs.closeSync(fd)));
