// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const fn = path.join(common.fixturesDir, 'elipses.txt');
const rangeFile = path.join(common.fixturesDir, 'x.txt');

{
  let paused = false;
  let bytesRead = 0;

  const file = fs.ReadStream(fn);
  const fileSize = fs.statSync(fn).size;

  assert.strictEqual(file.bytesRead, 0);

  file.on('open', common.mustCall(function(fd) {
    file.length = 0;
    assert.strictEqual('number', typeof fd);
    assert.strictEqual(file.bytesRead, 0);
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

    bytesRead += data.length;
    assert.strictEqual(file.bytesRead, bytesRead);

    paused = true;
    file.pause();

    setTimeout(function() {
      paused = false;
      file.resume();
    }, 10);
  });


  file.on('end', common.mustCall(function(chunk) {
    assert.strictEqual(bytesRead, fileSize);
    assert.strictEqual(file.bytesRead, fileSize);
  }));


  file.on('close', common.mustCall(function() {
    assert.strictEqual(bytesRead, fileSize);
    assert.strictEqual(file.bytesRead, fileSize);
  }));

  process.on('exit', function() {
    assert.strictEqual(file.length, 30000);
  });
}

{
  const file = fs.createReadStream(fn, {encoding: 'utf8'});
  file.length = 0;
  file.on('data', function(data) {
    assert.strictEqual('string', typeof data);
    file.length += data.length;

    for (let i = 0; i < data.length; i++) {
      // http://www.fileformat.info/info/unicode/char/2026/index.htm
      assert.strictEqual('\u2026', data[i]);
    }
  });

  file.on('close', common.mustCall());

  process.on('exit', function() {
    assert.strictEqual(file.length, 10000);
  });
}

{
  const file =
    fs.createReadStream(rangeFile, {bufferSize: 1, start: 1, end: 2});
  let contentRead = '';
  file.on('data', function(data) {
    contentRead += data.toString('utf-8');
  });
  file.on('end', common.mustCall(function(data) {
    assert.strictEqual(contentRead, 'yz');
  }));
}

{
  const file = fs.createReadStream(rangeFile, {bufferSize: 1, start: 1});
  file.data = '';
  file.on('data', function(data) {
    file.data += data.toString('utf-8');
  });
  file.on('end', common.mustCall(function() {
    assert.strictEqual(file.data, 'yz\n');
  }));
}

{
  // Ref: https://github.com/nodejs/node-v0.x-archive/issues/2320
  const file = fs.createReadStream(rangeFile, {bufferSize: 1.23, start: 1});
  file.data = '';
  file.on('data', function(data) {
    file.data += data.toString('utf-8');
  });
  file.on('end', common.mustCall(function() {
    assert.strictEqual(file.data, 'yz\n');
  }));
}

assert.throws(function() {
  fs.createReadStream(rangeFile, {start: 10, end: 2});
}, /"start" option must be <= "end" option/);

{
  const stream = fs.createReadStream(rangeFile, { start: 0, end: 0 });
  stream.data = '';

  stream.on('data', function(chunk) {
    stream.data += chunk;
  });

  stream.on('end', common.mustCall(function() {
    assert.strictEqual('x', stream.data);
  }));
}

{
  // pause and then resume immediately.
  const pauseRes = fs.createReadStream(rangeFile);
  pauseRes.pause();
  pauseRes.resume();
}

{
  let file = fs.createReadStream(rangeFile, {autoClose: false });
  let data = '';
  file.on('data', function(chunk) { data += chunk; });
  file.on('end', common.mustCall(function() {
    assert.strictEqual(data, 'xyz\n');
    process.nextTick(function() {
      assert(!file.closed);
      assert(!file.destroyed);
      fileNext();
    });
  }));

  function fileNext() {
    // This will tell us if the fd is usable again or not.
    file = fs.createReadStream(null, {fd: file.fd, start: 0 });
    file.data = '';
    file.on('data', function(data) {
      file.data += data;
    });
    file.on('end', common.mustCall(function(err) {
      assert.strictEqual(file.data, 'xyz\n');
    }));
    process.on('exit', function() {
      assert(file.closed);
      assert(file.destroyed);
    });
  }
}

{
  // Just to make sure autoClose won't close the stream because of error.
  const file = fs.createReadStream(null, {fd: 13337, autoClose: false });
  file.on('data', common.mustNotCall());
  file.on('error', common.mustCall());
  process.on('exit', function() {
    assert(!file.closed);
    assert(!file.destroyed);
    assert(file.fd);
  });
}

{
  // Make sure stream is destroyed when file does not exist.
  const file = fs.createReadStream('/path/to/file/that/does/not/exist');
  file.on('data', common.mustNotCall());
  file.on('error', common.mustCall());

  process.on('exit', function() {
    assert(!file.closed);
    assert(file.destroyed);
  });
}
