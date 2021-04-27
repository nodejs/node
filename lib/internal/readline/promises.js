'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  Promise,
  PromiseResolve,
} = primordials;

const { CSI } = require('internal/readline/utils');
const { validateInteger } = require('internal/validators');
const { isWritable } = require('internal/streams/utils');
const { codes: { ERR_INVALID_ARG_TYPE } } = require('internal/errors');

const {
  kClearToLineBeginning,
  kClearToLineEnd,
  kClearLine,
  kClearScreenDown,
} = CSI;

class Readline {
  #stream;
  #todo = [];

  constructor(stream) {
    if (!isWritable(stream))
      throw new ERR_INVALID_ARG_TYPE('stream', 'Writable', stream);
    this.#stream = stream;
  }

  /**
   * Moves the cursor to the x and y coordinate on the given stream.
   * @param {integer} x
   * @param {integer} [y]
   */
  cursorTo(x, y = undefined) {
    validateInteger(x, 'x');
    if (y != null) validateInteger(y, 'y');

    ArrayPrototypePush(
      this.#todo,
      y == null ? CSI`${x + 1}G` : CSI`${y + 1};${x + 1}H`
    );

    return this;
  }

  /**
   * Moves the cursor relative to its current location.
   * @param {integer} dx
   * @param {integer} dy
   */
  moveCursor(dx, dy) {
    if (dx || dy) {
      validateInteger(dx, 'dx');
      validateInteger(dy, 'dy');

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
      ArrayPrototypePush(this.#todo, data);
    }
    return this;
  }

  /**
   * Clears the current line the cursor is on:
   *   -1 for left of the cursor
   *   +1 for right of the cursor
   *    0 for the entire line
   */
  clearLine(dir) {
    validateInteger(dir, 'dir', -1, 1);

    ArrayPrototypePush(
      this.#todo,
      dir < 0 ? kClearToLineBeginning : dir > 0 ? kClearToLineEnd : kClearLine
    );
    return this;
  }

  /**
   * Clears the screen from the current position of the cursor down.
   */
  clearScreenDown() {
    ArrayPrototypePush(this.#todo, kClearScreenDown);
    return this;
  }

  commit() {
    return new Promise((resolve) => {
      this.#stream.write(ArrayPrototypeJoin(this.#todo, ''), resolve);
      this.#todo = [];
    });
  }
  rollback() {
    this.#todo = [];
    return PromiseResolve();
  }
}

module.exports = {
  Readline,
};
