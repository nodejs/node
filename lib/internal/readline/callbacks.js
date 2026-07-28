'use strict';

const {
  NumberIsNaN,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_CURSOR_POS,
  },
} = require('internal/errors');

const {
  validateFunction,
} = require('internal/validators');
const {
  CSI,
} = require('internal/readline/utils');

const {
  kClearLine,
  kClearScreenDown,
  kClearToLineBeginning,
  kClearToLineEnd,
} = CSI;


/**
 * moves the cursor to the x and y coordinate on the given stream
 */

function cursorTo(stream, x, y, callback) {
  if (callback !== undefined) {
    validateFunction(callback, 'callback');
  }

  if (typeof y === 'function') {
    callback = y;
    y = undefined;
  }

  if (NumberIsNaN(x)) throw new ERR_INVALID_ARG_VALUE('x', x);
  if (NumberIsNaN(y)) throw new ERR_INVALID_ARG_VALUE('y', y);

  if (stream == null || (typeof x !== 'number' && typeof y !== 'number')) {
    if (typeof callback === 'function') process.nextTick(callback, null);
    return true;
  }

  if (typeof x !== 'number') throw new ERR_INVALID_CURSOR_POS();

  const data = typeof y !== 'number' ? CSI`${x + 1}G` : CSI`${y + 1};${x + 1}H`;
  return stream.write(data, callback);
}

/**
 * moves the cursor relative to its current location
 */

function moveCursor(stream, dx, dy, callback) {
  if (callback !== undefined) {
    validateFunction(callback, 'callback');
  }

  if (stream == null || !(dx || dy)) {
    if (typeof callback === 'function') process.nextTick(callback, null);
    return true;
  }

  let data = '';

  if (dx < 0) {
    data += CSI`${-dx}D`;
  } else if (dx > 0) {
    data += CSI`${dx}C`;
  }

  if (dy < 0) {
    data += CSI`${-dy}A`;
  } else if (dy > 0) {
    data += CSI`${dy}B`;
  }

  return stream.write(data, callback);
}

/**
 * clears the current line the cursor is on:
 *   -1 for left of the cursor
 *   +1 for right of the cursor
 *    0 for the entire line
 */

function clearLine(stream, dir, callback) {
  if (callback !== undefined) {
    validateFunction(callback, 'callback');
  }

  if (stream === null || stream === undefined) {
    if (typeof callback === 'function') process.nextTick(callback, null);
    return true;
  }

  const type =
    dir < 0 ? kClearToLineBeginning : dir > 0 ? kClearToLineEnd : kClearLine;
  return stream.write(type, callback);
}

/**
 * clears the screen from the current position of the cursor down
 */

function clearScreenDown(stream, callback) {
  if (callback !== undefined) {
    validateFunction(callback, 'callback');
  }

  if (stream === null || stream === undefined) {
    if (typeof callback === 'function') process.nextTick(callback, null);
    return true;
  }

  return stream.write(kClearScreenDown, callback);
}

module.exports = {
  clearLine,
  clearScreenDown,
  cursorTo,
  moveCursor,
};
