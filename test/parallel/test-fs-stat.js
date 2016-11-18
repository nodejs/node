/* eslint-disable strict */
const common = require('../common');
var assert = require('assert');
var fs = require('fs');

fs.stat('.', common.mustCall(function(err, stats) {
  assert.ifError(err);
  assert.ok(stats.mtime instanceof Date);
  assert.strictEqual(this, global);
}));

fs.stat('.', common.mustCall(function(err, stats) {
  assert.ok(stats.hasOwnProperty('blksize'));
  assert.ok(stats.hasOwnProperty('blocks'));
}));

fs.lstat('.', common.mustCall(function(err, stats) {
  assert.ifError(err);
  assert.ok(stats.mtime instanceof Date);
  assert.strictEqual(this, global);
}));

// fstat
fs.open('.', 'r', undefined, common.mustCall(function(err, fd) {
  assert.ok(!err);
  assert.ok(fd);

  fs.fstat(fd, common.mustCall(function(err, stats) {
    assert.ifError(err);
    assert.ok(stats.mtime instanceof Date);
    fs.close(fd);
    assert.strictEqual(this, global);
  }));

  assert.strictEqual(this, global);
}));

// fstatSync
fs.open('.', 'r', undefined, common.mustCall(function(err, fd) {
  var stats;
  try {
    stats = fs.fstatSync(fd);
  } catch (err) {
    common.fail(err);
  }
  if (stats) {
    console.dir(stats);
    assert.ok(stats.mtime instanceof Date);
  }
  fs.close(fd);
}));

console.log(`stating:  ${__filename}`);
fs.stat(__filename, common.mustCall(function(err, s) {
  assert.ifError(err);

  console.dir(s);

  console.log('isDirectory: ' + JSON.stringify(s.isDirectory()));
  assert.equal(false, s.isDirectory());

  console.log('isFile: ' + JSON.stringify(s.isFile()));
  assert.equal(true, s.isFile());

  console.log('isSocket: ' + JSON.stringify(s.isSocket()));
  assert.equal(false, s.isSocket());

  console.log('isBlockDevice: ' + JSON.stringify(s.isBlockDevice()));
  assert.equal(false, s.isBlockDevice());

  console.log('isCharacterDevice: ' + JSON.stringify(s.isCharacterDevice()));
  assert.equal(false, s.isCharacterDevice());

  console.log('isFIFO: ' + JSON.stringify(s.isFIFO()));
  assert.equal(false, s.isFIFO());

  console.log('isSymbolicLink: ' + JSON.stringify(s.isSymbolicLink()));
  assert.equal(false, s.isSymbolicLink());

  assert.ok(s.mtime instanceof Date);
}));
