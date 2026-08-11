'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

// Invalid fds must throw a JS exception from the Socket constructor rather than
// aborting the process inside libuv (see https://github.com/nodejs/node/issues/63308).

assert.throws(
  () => new net.Socket({ fd: -1 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  }
);

common.runWithInvalidFD((fd) => {
  assert.throws(
    () => {
      new net.Socket({
        fd,
        readable: false,
        writable: true,
      });
    },
    {
      code: 'EBADF',
      syscall: 'fstat',
    }
  );
});

// Wrapping arbitrary existing fds is unsupported on Windows.
if (common.isWindows)
  return;

// Iterating arbitrary fds must not abort the process. Unsupported fds throw;
// in-use libuv fds return EEXIST from open; others may open and emit errors.
{
  let fd = 3;
  while (fd < 64) {
    try {
      const stream = new net.Socket({
        fd,
        readable: false,
        writable: true,
      });
      stream.on('error', () => {});
      stream.write('might crash');
      stream.destroy();
    } catch {
      // Expected for unsupported / invalid / already-watched descriptors.
    }
    fd += 1;
  }
}

setImmediate(common.mustCall(() => {
  // If libuv aborted, we never reach here.
}));
