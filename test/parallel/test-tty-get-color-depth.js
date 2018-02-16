'use strict';

const common = require('../common');
const assert = require('assert').strict;
/* eslint-disable no-restricted-properties */
const { WriteStream } = require('tty');

const fd = common.getTTYfd();

// Give up if we did not find a tty
if (fd === -1)
  common.skip();

const writeStream = new WriteStream(fd);

let depth = writeStream.getColorDepth();

assert.equal(typeof depth, 'number');
assert(depth >= 1 && depth <= 24);

// If the terminal does not support colors, skip the rest
if (depth === 1)
  common.skip();

assert.notEqual(writeStream.getColorDepth({ TERM: 'dumb' }), depth);

// Deactivate colors
const tmp = process.env.NODE_DISABLE_COLORS;
process.env.NODE_DISABLE_COLORS = 1;

depth = writeStream.getColorDepth();

assert.equal(depth, 1);

process.env.NODE_DISABLE_COLORS = tmp;
