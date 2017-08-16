'use strict';
const common = require('../common');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const fn = path.join(common.fixturesDir, 'elipses.txt');
const rangeFile = path.join(common.fixturesDir, 'x.txt');

{
  let paused = false;

  const file = fs.ReadStream(fn);

  file.on('open', common.mustCall(function(fd) {
    file.length = 0;
    assert.strictEqual(typeof fd, 'number');
    assert.ok(file.readable);

    // GH-535
    file.pause();
    file.resume();
    file.pause();
    file.resume();
  }));

  file.on('data', function(data) {
    assert.ok(data instanceof Buffer);
    assert.ok(!paused);
    file.length += data.length;

    paused = true;
    file.pause();

    setTimeout(function() {
      paused = false;
      file.resume();
    }, 10);
  });


  file.on('end', common.mustCall());


  file.on('close', common.mustCall(function() {
    assert.strictEqual(file.length, 30000);
  }));
}

{
  const file = fs.createReadStream(fn, Object.create({ encoding: 'utf8' }));
  file.length = 0;
  file.on('data', function(data) {
    assert.strictEqual(typeof data, 'string');
    file.length += data.length;

    for (let i = 0; i < data.length; i++) {
      // http://www.fileformat.info/info/unicode/char/2026/index.htm
      assert.strictEqual(data[i], '\u2026');
    }
  });

  file.on('close', common.mustCall(function() {
    assert.strictEqual(file.length, 10000);
  }));
}

{
  const options = Object.create({ bufferSize: 1, start: 1, end: 2 });
  const file = fs.createReadStream(rangeFile, options);
  assert.strictEqual(file.start, 1);
  assert.strictEqual(file.end, 2);
  let contentRead = '';
  file.on('data', function(data) {
    contentRead += data.toString('utf-8');
  });
  file.on('end', common.mustCall(function() {
    assert.strictEqual(contentRead, 'yz');
  }));
}

{
  const options = Object.create({ bufferSize: 1, start: 1 });
  const file = fs.createReadStream(rangeFile, options);
  assert.strictEqual(file.start, 1);
  file.data = '';
  file.on('data', function(data) {
    file.data += data.toString('utf-8');
  });
  file.on('end', common.mustCall(function() {
    assert.strictEqual(file.data, 'yz\n');
  }));
}

// https://github.com/joyent/node/issues/2320
{
  const options = Object.create({ bufferSize: 1.23, start: 1 });
  const file = fs.createReadStream(rangeFile, options);
  assert.strictEqual(file.start, 1);
  file.data = '';
  file.on('data', function(data) {
    file.data += data.toString('utf-8');
  });
  file.on('end', common.mustCall(function() {
    assert.strictEqual(file.data, 'yz\n');
  }));
}

{
  assert.throws(function() {
    fs.createReadStream(rangeFile, Object.create({ start: 10, end: 2 }));
  }, /"start" option must be <= "end" option/);
}

{
  const options = Object.create({ start: 0, end: 0 });
  const stream = fs.createReadStream(rangeFile, options);
  assert.strictEqual(stream.start, 0);
  assert.strictEqual(stream.end, 0);
  stream.data = '';

  stream.on('data', function(chunk) {
    stream.data += chunk;
  });

  stream.on('end', common.mustCall(function() {
    assert.strictEqual(stream.data, 'x');
  }));
}

// pause and then resume immediately.
{
  const pauseRes = fs.createReadStream(rangeFile);
  pauseRes.pause();
  pauseRes.resume();
}

{
  let data = '';
  let file =
    fs.createReadStream(rangeFile, Object.create({ autoClose: false }));
  assert.strictEqual(file.autoClose, false);
  file.on('data', (chunk) => { data += chunk; });
  file.on('end', common.mustCall(function() {
    process.nextTick(common.mustCall(function() {
      assert(!file.closed);
      assert(!file.destroyed);
      assert.strictEqual(data, 'xyz\n');
      fileNext();
    }));
  }));

  function fileNext() {
    // This will tell us if the fd is usable again or not.
    file = fs.createReadStream(null, Object.create({ fd: file.fd, start: 0 }));
    file.data = '';
    file.on('data', function(data) {
      file.data += data;
    });
    file.on('end', common.mustCall(function() {
      assert.strictEqual(file.data, 'xyz\n');
    }));
  }
  process.on('exit', function() {
    assert(file.closed);
    assert(file.destroyed);
  });
}

// Just to make sure autoClose won't close the stream because of error.
{
  const options = Object.create({ fd: 13337, autoClose: false });
  const file = fs.createReadStream(null, options);
  file.on('data', common.mustNotCall());
  file.on('error', common.mustCall());
  process.on('exit', function() {
    assert(!file.closed);
    assert(!file.destroyed);
    assert(file.fd);
  });
}

// Make sure stream is destroyed when file does not exist.
{
  const file = fs.createReadStream('/path/to/file/that/does/not/exist');
  file.on('data', common.mustNotCall());
  file.on('error', common.mustCall());

  process.on('exit', function() {
    assert(!file.closed);
    assert(file.destroyed);
  });
}
