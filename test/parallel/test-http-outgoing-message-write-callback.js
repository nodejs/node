// Flags: --expose-internals

'use strict';

const common = require('../common');

// This test ensures that the callback of `OutgoingMessage.prototype.write()` is
// called also when writing empty chunks or when the message has no body.

const assert = require('assert');
const http = require('http');
const stream = require('stream');
const {
  kInternalWritev,
  kRawWritev,
} = require('internal/streams/utils');

for (const method of ['GET, HEAD']) {
  const expected = ['a', 'b', '', Buffer.alloc(0), 'c'];
  const results = [];

  const writable = new stream.Writable({
    write(chunk, encoding, callback) {
      callback();
    }
  });

  const res = new http.ServerResponse({
    method: method,
    httpVersionMajor: 1,
    httpVersionMinor: 1
  });

  res.assignSocket(writable);

  for (const chunk of expected) {
    res.write(chunk, () => {
      results.push(chunk);
    });
  }

  res.end(common.mustCall(() => {
    assert.deepStrictEqual(results, expected);
  }));
}

// A custom socket write may invoke its callback synchronously. The end()
// callback must be registered before that custom path can emit 'finish'.
{
  const writable = new stream.Writable({
    write(chunk, encoding, callback) {
      callback();
    }
  });
  writable.write = function(chunk, encoding, callback) {
    callback();
    return true;
  };
  writable[kInternalWritev] = common.mustNotCall();
  writable[kRawWritev] = common.mustNotCall();

  const res = new http.ServerResponse({
    method: 'GET',
    httpVersionMajor: 1,
    httpVersionMinor: 1
  });
  res.assignSocket(writable);

  let ended = false;
  res.end('body', common.mustCall(() => ended = true));
  assert.strictEqual(ended, true);
}

// Flushing output queued before socket assignment must preserve the same
// public write override instead of switching to an internal vector path.
{
  const writes = [];
  const writable = new stream.Writable({
    write(chunk, encoding, callback) {
      callback();
    }
  });
  writable.write = function(chunk, encoding, callback) {
    writes.push(chunk);
    callback();
    return true;
  };
  writable[kInternalWritev] = common.mustNotCall();
  writable[kRawWritev] = common.mustNotCall();

  const res = new http.ServerResponse({
    method: 'GET',
    httpVersionMajor: 1,
    httpVersionMinor: 1
  });
  res.write('A');
  res.end('B', common.mustCall());
  assert(res.outputData.length > 1);
  res.assignSocket(writable);
  assert(writes.length > 1);
}
