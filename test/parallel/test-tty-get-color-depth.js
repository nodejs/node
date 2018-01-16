'use strict';

const common = require('../common');
const assert = require('assert').strict;
/* eslint-disable no-restricted-properties */
const { openSync } = require('fs');
const tty = require('tty');

const { WriteStream } = require('tty');

// Do our best to grab a tty fd.
function getTTYfd() {
  const ttyFd = [0, 1, 2, 4, 5].find(tty.isatty);
  if (ttyFd === undefined) {
    try {
      return openSync('/dev/tty');
    } catch (e) {
      // There aren't any tty fd's available to use.
      return -1;
    }
  }
  return ttyFd;
}

const fd = getTTYfd();

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
