/* eslint-disable strict */
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var got_error = false;
var success_count = 0;

fs.stat('.', function(err, stats) {
  if (err) {
    got_error = true;
  } else {
    console.dir(stats);
    assert.ok(stats.mtime instanceof Date);
    success_count++;
  }
  assert(this === global);
});

fs.stat('.', function(err, stats) {
  assert.ok(stats.hasOwnProperty('blksize'));
  assert.ok(stats.hasOwnProperty('blocks'));
});

fs.lstat('.', function(err, stats) {
  if (err) {
    got_error = true;
  } else {
    console.dir(stats);
    assert.ok(stats.mtime instanceof Date);
    success_count++;
  }
  assert(this === global);
});

// fstat
fs.open('.', 'r', undefined, function(err, fd) {
  assert.ok(!err);
  assert.ok(fd);

  fs.fstat(fd, function(err, stats) {
    if (err) {
      got_error = true;
    } else {
      console.dir(stats);
      assert.ok(stats.mtime instanceof Date);
      success_count++;
      fs.close(fd);
    }
    assert(this === global);
  });

  assert(this === global);
});

// fstatSync
fs.open('.', 'r', undefined, function(err, fd) {
  var stats;
  try {
    stats = fs.fstatSync(fd);
  } catch (err) {
    got_error = true;
  }
  if (stats) {
    console.dir(stats);
    assert.ok(stats.mtime instanceof Date);
    success_count++;
  }
  fs.close(fd);
});

console.log('stating: ' + __filename);
fs.stat(__filename, function(err, s) {
  if (err) {
    got_error = true;
  } else {
    console.dir(s);
    success_count++;

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
  }
});

process.on('exit', function() {
  assert.equal(5, success_count);
  assert.equal(false, got_error);
});

