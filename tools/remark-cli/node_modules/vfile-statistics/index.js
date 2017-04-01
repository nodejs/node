/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module vfile-statistics
 * @fileoverview Count vfile messages per category.
 */

'use strict';

/* Expose. */
module.exports = statistics;

/** Get stats for a file, list of files, or list of messages. */
function statistics(files) {
  var total = 0;
  var result = {true: 0, false: 0};

  count(files);

  return {fatal: result.true, nonfatal: result.false, total: total};

  function count(value) {
    var index;
    var length;

    if (!value) {
      return;
    }

    if (value.messages) {
      count(value.messages);
    } else if (value[0] && value[0].messages) {
      index = -1;
      length = value.length;

      while (++index < length) {
        count(value[index].messages);
      }
    } else {
      index = -1;
      length = value.length;

      while (++index < length) {
        result[Boolean(value[index].fatal)]++;
        total++;
      }
    }
  }
}
