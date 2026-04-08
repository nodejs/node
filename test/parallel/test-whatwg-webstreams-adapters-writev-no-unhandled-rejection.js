// Flags: --no-warnings --expose-internals
'use strict';

// Regression test for https://github.com/nodejs/node/issues/62199
// Verifies that the writev done callback in webstream adapters does not
// throw a TypeError when the error comes from writer.ready rejection
// (a single error) rather than SafePromiseAll (an array of errors).

const common = require('../common');

const {
  TransformStream,
  WritableStream,
} = require('stream/web');

const {
  newStreamWritableFromWritableStream,
  newStreamDuplexFromReadableWritablePair,
} = require('internal/webstreams/adapters');

const { Duplex } = require('stream');

// Test 1: Duplex.fromWeb writev path should not cause unhandled rejection
{
  const output = Duplex.fromWeb(new TransformStream());
  output.on('error', common.mustCall());
  output.cork();
  output.write('test');
  output.write('test');
  output.uncork();
  output.destroy();
}

// Test 2: newStreamDuplexFromReadableWritablePair writev path
{
  const transform = new TransformStream();
  const duplex = newStreamDuplexFromReadableWritablePair(transform);
  duplex.on('error', common.mustCall());
  duplex.cork();
  duplex.write('test');
  duplex.write('test');
  duplex.uncork();
  duplex.destroy();
}

// Test 3: newStreamWritableFromWritableStream writev path
{
  const writableStream = new WritableStream();
  const writable = newStreamWritableFromWritableStream(writableStream);
  writable.on('error', common.mustCall());
  writable.cork();
  writable.write('test');
  writable.write('test');
  writable.uncork();
  writable.destroy();
}
