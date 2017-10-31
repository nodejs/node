'use strict';

/* Expose. */
module.exports = generated;

/* Detect if a node was available in the original document. */
function generated(node) {
  var position = optional(optional(node).position);
  var start = optional(position.start);
  var end = optional(position.end);

  return !start.line || !start.column || !end.line || !end.column;
}

/* Return `value` if it’s an object, an empty object
 * otherwise. */
function optional(value) {
  return value && typeof value === 'object' ? value : {};
}
