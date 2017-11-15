'use strict';

module.exports = quote;

/* Wrap `value` with double quotes, and escape internal
 * double quotes. */
function quote(value) {
  return '"' + String(value).replace(/"/g, '\\"') + '"';
}
