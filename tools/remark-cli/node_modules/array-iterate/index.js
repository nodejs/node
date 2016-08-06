/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module array-iterate
 * @fileoverview `forEach` with the possibility to change the
 *   next position.
 */

'use strict';

/* Dependencies. */
var has = require('has');

/* Expose. */
module.exports = iterate;

/**
 * `Array#forEach()` with the possibility to change
 * the next position.
 *
 * @param {{length: number}} values - Values.
 * @param {arrayIterate~callback} callback - Callback given to `iterate`.
 * @param {*?} [context] - Context object to use when invoking `callback`.
 */
function iterate(values, callback, context) {
  var index = -1;
  var result;

  if (!values) {
    throw new Error('Iterate requires that |this| not be ' + values);
  }

  if (!has(values, 'length')) {
    throw new Error('Iterate requires that |this| has a `length`');
  }

  if (typeof callback !== 'function') {
    throw new Error('`callback` must be a function');
  }

  /* The length might change, so we do not cache it. */
  while (++index < values.length) {
    /* Skip missing values. */
    if (!(index in values)) {
      continue;
    }

    result = callback.call(context, values[index], index, values);

    /*
     * If `callback` returns a `number`, move `index` over to
     * `number`.
     */

    if (typeof result === 'number') {
      /* Make sure that negative numbers do not break the loop. */
      if (result < 0) {
        index = 0;
      }

      index = result - 1;
    }
  }
}
