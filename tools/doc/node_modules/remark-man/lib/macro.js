'use strict';

module.exports = macro;

/* Compile a roff macro. */
function macro(name, value) {
  var val = value || '';

  if (val && val.charAt(0) !== '\n') {
    val = ' ' + val;
  }

  return '.' + name + val;
}
