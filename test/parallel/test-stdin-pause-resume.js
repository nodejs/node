'use strict';
require('../common');
console.error('before opening stdin');
process.stdin.resume();
console.error('stdin opened');
setTimeout(function() {
  console.error('pausing stdin');
  process.stdin.pause();
  setTimeout(function() {
    console.error('opening again');
    process.stdin.resume();
    setTimeout(function() {
      console.error('pausing again');
      process.stdin.pause();
      console.error('should exit now');
    }, 1);
  }, 1);
}, 1);
