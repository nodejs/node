'use strict';
const common = require('../common');
const assert = require('assert');

const path = require('path');
const fs = require('fs');
const fn = path.join(common.fixturesDir, 'elipses.txt');
const rangeFile = path.join(common.fixturesDir, 'x.txt');

let paused = false;

{
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


  file.on('end', common.mustCall(function() {}));


  file.on('close', common.mustCall(function() {
    assert.strictEqual(file.length, 30000);
  }));
}

{
  const file3 = fs.createReadStream(fn, Object.create({encoding: 'utf8'}));
  file3.length = 0;
  file3.on('data', function(data) {
    assert.strictEqual(typeof data, 'string');
    file3.length += data.length;

    for (let i = 0; i < data.length; i++) {
      // http://www.fileformat.info/info/unicode/char/2026/index.htm
      assert.strictEqual(data[i], '\u2026');
    }
  });

  file3.on('close', common.mustCall(function() {
    assert.strictEqual(file3.length, 10000);
  }));
}

{
  const options = Object.create({bufferSize: 1, start: 1, end: 2});
  const file4 = fs.createReadStream(rangeFile, options);
  assert.strictEqual(file4.start, 1);
  assert.strictEqual(file4.end, 2);
  let contentRead = '';
  file4.on('data', function(data) {
    contentRead += data.toString('utf-8');
  });
  file4.on('end', common.mustCall(function() {
    assert.strictEqual(contentRead, 'yz');
  }));
}

{
  const options = Object.create({bufferSize: 1, start: 1});
  const file5 = fs.createReadStream(rangeFile, options);
  assert.strictEqual(file5.start, 1);
  file5.data = '';
  file5.on('data', function(data) {
    file5.data += data.toString('utf-8');
  });
  file5.on('end', common.mustCall(function() {
    assert.strictEqual(file5.data, 'yz\n');
  }));
}

// https://github.com/joyent/node/issues/2320
{
  const options = Object.create({bufferSize: 1.23, start: 1});
  const file6 = fs.createReadStream(rangeFile, options);
  assert.strictEqual(file6.start, 1);
  file6.data = '';
  file6.on('data', function(data) {
    file6.data += data.toString('utf-8');
  });
  file6.on('end', common.mustCall(function() {
    assert.strictEqual(file6.data, 'yz\n');
  }));
}

{
  assert.throws(function() {
    fs.createReadStream(rangeFile, Object.create({start: 10, end: 2}));
  }, /"start" option must be <= "end" option/);
}

{
  const options = Object.create({start: 0, end: 0});
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
  let file7 =
    fs.createReadStream(rangeFile, Object.create({autoClose: false }));
  assert.strictEqual(file7.autoClose, false);
  file7.on('data', function() {});
  file7.on('end', common.mustCall(function() {
    process.nextTick(common.mustCall(function() {
      assert(!file7.closed);
      assert(!file7.destroyed);
      file7Next();
    }));
  }));

  function file7Next() {
    // This will tell us if the fd is usable again or not.
    file7 = fs.createReadStream(null, Object.create({fd: file7.fd, start: 0 }));
    file7.data = '';
    file7.on('data', function(data) {
      file7.data += data;
    });
    file7.on('end', common.mustCall(function() {
      assert.strictEqual(file7.data, 'xyz\n');
    }));
  }
  process.on('exit', function() {
    assert(file7.closed);
    assert(file7.destroyed);
  });
}

// Just to make sure autoClose won't close the stream because of error.
{
  const options = Object.create({fd: 13337, autoClose: false});
  const file8 = fs.createReadStream(null, options);
  file8.on('data', function() {});
  file8.on('error', common.mustCall(function() {}));
  process.on('exit', function() {
    assert(!file8.closed);
    assert(!file8.destroyed);
    assert(file8.fd);
  });
}

// Make sure stream is destroyed when file does not exist.
{
  const file9 = fs.createReadStream('/path/to/file/that/does/not/exist');
  file9.on('data', function() {});
  file9.on('error', common.mustCall(function() {}));

  process.on('exit', function() {
    assert(!file9.closed);
    assert(file9.destroyed);
  });
}
