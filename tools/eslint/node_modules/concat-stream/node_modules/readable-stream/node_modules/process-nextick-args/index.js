'use strict';
module.exports = nextTick;

function nextTick(fn) {
  var args = new Array(arguments.length - 1);
  var i = 0;
  while (i < arguments.length) {
    args[i++] = arguments[i];
  }
  process.nextTick(function afterTick() {
    fn.apply(null, args);
  });
}
