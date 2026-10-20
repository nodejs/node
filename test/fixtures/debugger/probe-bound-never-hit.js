'use strict';

function neverCalled() {
  console.log('unreachable');
  console.log('also unreachable');
  console.log('still unreachable');
}

console.log('reached');
module.exports = neverCalled;  // Keep the function alive.
