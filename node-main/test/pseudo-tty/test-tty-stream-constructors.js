'use strict';
require('../common');
const assert = require('assert');
const { ReadStream, WriteStream } = require('tty');

{
  // Verify that tty.ReadStream can be constructed without new.
  const stream = ReadStream(0);

  stream.unref();
  assert(stream instanceof ReadStream);
  assert.strictEqual(stream.isTTY, true);
}

{
  // Verify that tty.WriteStream can be constructed without new.
  const stream = WriteStream(1);

  assert(stream instanceof WriteStream);
  assert.strictEqual(stream.isTTY, true);
}
