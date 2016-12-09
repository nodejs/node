'use strict';
const common = require('../common');
const assert = require('assert');

const path = require('path');
const fs = require('fs');
const fn = path.join(common.fixturesDir, 'elipses.txt');
const rangeFile = path.join(common.fixturesDir, 'x.txt');

var callbacks = { open: 0, end: 0, close: 0 };

var paused = false;
var bytesRead = 0;

var file = fs.ReadStream(fn);
var fileSize = fs.statSync(fn).size;

assert.strictEqual(file.bytesRead, 0);

file.on('open', function(fd) {
  file.length = 0;
  callbacks.open++;
  assert.strictEqual('number', typeof fd);
  assert.strictEqual(file.bytesRead, 0);
  assert.ok(file.readable);

  // GH-535
  file.pause();
  file.resume();
  file.pause();
  file.resume();
});

file.on('data', function(data) {
  assert.ok(data instanceof Buffer);
  assert.ok(!paused);
  file.length += data.length;

  bytesRead += data.length;
  assert.strictEqual(file.bytesRead, bytesRead);

  paused = true;
  file.pause();

  setTimeout(function() {
    paused = false;
    file.resume();
  }, 10);
});


file.on('end', function(chunk) {
  assert.strictEqual(bytesRead, fileSize);
  assert.strictEqual(file.bytesRead, fileSize);
  callbacks.end++;
});


file.on('close', function() {
  assert.strictEqual(bytesRead, fileSize);
  assert.strictEqual(file.bytesRead, fileSize);
  callbacks.close++;

  //assert.equal(fs.readFileSync(fn), fileContent);
});

var file3 = fs.createReadStream(fn, {encoding: 'utf8'});
file3.length = 0;
file3.on('data', function(data) {
  assert.strictEqual('string', typeof data);
  file3.length += data.length;

  for (var i = 0; i < data.length; i++) {
    // http://www.fileformat.info/info/unicode/char/2026/index.htm
    assert.strictEqual('\u2026', data[i]);
  }
});

file3.on('close', function() {
  callbacks.close++;
});

process.on('exit', function() {
  assert.strictEqual(1, callbacks.open);
  assert.strictEqual(1, callbacks.end);
  assert.strictEqual(2, callbacks.close);
  assert.strictEqual(30000, file.length);
  assert.strictEqual(10000, file3.length);
  console.error('ok');
});

var file4 = fs.createReadStream(rangeFile, {bufferSize: 1, start: 1, end: 2});
var contentRead = '';
file4.on('data', function(data) {
  contentRead += data.toString('utf-8');
});
file4.on('end', function(data) {
  assert.strictEqual(contentRead, 'yz');
});

var file5 = fs.createReadStream(rangeFile, {bufferSize: 1, start: 1});
file5.data = '';
file5.on('data', function(data) {
  file5.data += data.toString('utf-8');
});
file5.on('end', function() {
  assert.strictEqual(file5.data, 'yz\n');
});

// https://github.com/joyent/node/issues/2320
var file6 = fs.createReadStream(rangeFile, {bufferSize: 1.23, start: 1});
file6.data = '';
file6.on('data', function(data) {
  file6.data += data.toString('utf-8');
});
file6.on('end', function() {
  assert.strictEqual(file6.data, 'yz\n');
});

assert.throws(function() {
  fs.createReadStream(rangeFile, {start: 10, end: 2});
}, /"start" option must be <= "end" option/);

var stream = fs.createReadStream(rangeFile, { start: 0, end: 0 });
stream.data = '';

stream.on('data', function(chunk) {
  stream.data += chunk;
});

stream.on('end', function() {
  assert.strictEqual('x', stream.data);
});

// pause and then resume immediately.
var pauseRes = fs.createReadStream(rangeFile);
pauseRes.pause();
pauseRes.resume();

var file7 = fs.createReadStream(rangeFile, {autoClose: false });
file7.on('data', function() {});
file7.on('end', function() {
  process.nextTick(function() {
    assert(!file7.closed);
    assert(!file7.destroyed);
    file7Next();
  });
});

function file7Next() {
  // This will tell us if the fd is usable again or not.
  file7 = fs.createReadStream(null, {fd: file7.fd, start: 0 });
  file7.data = '';
  file7.on('data', function(data) {
    file7.data += data;
  });
  file7.on('end', function(err) {
    assert.strictEqual(file7.data, 'xyz\n');
  });
}

// Just to make sure autoClose won't close the stream because of error.
var file8 = fs.createReadStream(null, {fd: 13337, autoClose: false });
file8.on('data', function() {});
file8.on('error', common.mustCall(function() {}));

// Make sure stream is destroyed when file does not exist.
var file9 = fs.createReadStream('/path/to/file/that/does/not/exist');
file9.on('data', function() {});
file9.on('error', common.mustCall(function() {}));

process.on('exit', function() {
  assert(file7.closed);
  assert(file7.destroyed);

  assert(!file8.closed);
  assert(!file8.destroyed);
  assert(file8.fd);

  assert(!file9.closed);
  assert(file9.destroyed);
});
