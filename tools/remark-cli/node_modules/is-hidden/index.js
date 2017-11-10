'use strict';

/* Expose. */
module.exports = hidden;

/* Check if `filename` is hidden (starts with a dot). */
function hidden(filename) {
  if (typeof filename !== 'string') {
    throw new Error('Expected string');
  }

  return filename.charAt(0) === '.';
}
