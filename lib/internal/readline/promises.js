'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  Promise,
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
   * @returns {Readline} this
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
   * @returns {Readline} this
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
   * Clears the current line the cursor is on.
   * @param {-1|0|1} dir Direction to clear:
   *   -1 for left of the cursor
   *   +1 for right of the cursor
   *    0 for the entire line
   * @returns {Readline} this
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
   * @returns {Readline} this
   */
  clearScreenDown() {
    ArrayPrototypePush(this.#todo, kClearScreenDown);
    return this;
  }

  /**
   * Sends all the pending actions to the associated `stream` and clears the
   * internal list of pending actions.
   * @returns {Promise<void>} Resolves when all pending actions have been
   * flushed to the associated `stream`.
   */
  commit() {
    return new Promise((resolve) => {
      this.#stream.write(ArrayPrototypeJoin(this.#todo, ''), resolve);
      this.#todo = [];
    });
  }

  /**
   * Clears the internal list of pending actions without sending it to the
   * associated `stream`.
   * @returns {Readline} this
   */
  rollback() {
    this.#todo = [];
    return this;
  }
}

module.exports = {
  Readline,
};
