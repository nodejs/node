'use strict';

const common = require('../common');
const assert = require('assert').strict;
/* eslint-disable no-restricted-properties */
const { WriteStream } = require('tty');

const fd = common.getTTYfd();
const writeStream = new WriteStream(fd);

{
  const depth = writeStream.getColorDepth();

  assert.equal(typeof depth, 'number');
  assert(depth >= 1 && depth <= 24);

  if (depth === 1) {
    // Terminal does not support colors, compare to a value that would.
    assert.notEqual(writeStream.getColorDepth({ COLORTERM: '1' }), depth);
  } else {
    // Terminal supports colors, compare to a value that would not.
    assert.notEqual(writeStream.getColorDepth({ TERM: 'dumb' }), depth);
  }
}

// Deactivate colors
{
  const tmp = process.env.NODE_DISABLE_COLORS;
  process.env.NODE_DISABLE_COLORS = 1;

  const depth = writeStream.getColorDepth();

  assert.equal(depth, 1);

  process.env.NODE_DISABLE_COLORS = tmp;
}
