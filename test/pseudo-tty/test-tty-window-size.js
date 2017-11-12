'use strict';
const common = require('../common');
const assert = require('assert');
const { WriteStream } = require('tty');
const { TTY } = process.binding('tty_wrap');
const getWindowSize = TTY.prototype.getWindowSize;

function monkeyPatchGetWindowSize(fn) {
  TTY.prototype.getWindowSize = function() {
    TTY.prototype.getWindowSize = getWindowSize;
    return fn.apply(this, arguments);
  };
}

{
  // tty.WriteStream constructor does not define columns and rows if an error
  // occurs while retrieving the window size from the handle.
  monkeyPatchGetWindowSize(function() {
    return -1;
  });

  const stream = WriteStream(1);

  assert(stream instanceof WriteStream);
  assert.strictEqual(stream.columns, undefined);
  assert.strictEqual(stream.rows, undefined);
}

{
  // _refreshSize() emits an error if an error occurs while retrieving the
  // window size from the handle.
  const stream = WriteStream(1);

  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.syscall, 'getWindowSize');
  }));

  monkeyPatchGetWindowSize(function() {
    return -1;
  });

  stream._refreshSize();
}

{
  // _refreshSize() emits a 'resize' event when the window size changes.
  monkeyPatchGetWindowSize(function(size) {
    size[0] = 80;
    size[1] = 24;
    return 0;
  });

  const stream = WriteStream(1);

  stream.on('resize', common.mustCall(() => {
    assert.strictEqual(stream.columns, 82);
    assert.strictEqual(stream.rows, 26);
  }));

  assert.strictEqual(stream.columns, 80);
  assert.strictEqual(stream.rows, 24);

  monkeyPatchGetWindowSize(function(size) {
    size[0] = 82;
    size[1] = 26;
    return 0;
  });

  stream._refreshSize();
}

{
  // WriteStream.prototype.getWindowSize() returns the current columns and rows.
  monkeyPatchGetWindowSize(function(size) {
    size[0] = 99;
    size[1] = 32;
    return 0;
  });

  const stream = WriteStream(1);

  assert.strictEqual(stream.columns, 99);
  assert.strictEqual(stream.rows, 32);
  assert.deepStrictEqual(stream.getWindowSize(), [99, 32]);
}
