/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module bail
 * @fileoverview Throw a given error.
 */

'use strict';

/* Expose. */
module.exports = bail;

/**
 * Throw a given error.
 *
 * @example
 *   bail();
 *
 * @example
 *   bail(new Error('failure'));
 *   // Error: failure
 *   //     at repl:1:6
 *   //     at REPLServer.defaultEval (repl.js:154:27)
 *   //     ...
 *
 * @param {Error?} [err] - Optional error.
 * @throws {Error} - `err`, when given.
 */
function bail(err) {
  if (err) {
    throw err;
  }
}
