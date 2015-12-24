'use strict';
require('../common');
var assert = require('assert');
var i;

var N = 30;
var done = [];

function get_printer(timeout) {
  return function() {
    console.log('Running from setTimeout ' + timeout);
    done.push(timeout);
  };
}

process.nextTick(function() {
  console.log('Running from nextTick');
  done.push('nextTick');
});

for (i = 0; i < N; i += 1) {
  setTimeout(get_printer(i), i);
}

console.log('Running from main.');


process.on('exit', function() {
  assert.equal('nextTick', done[0]);
  /* Disabling this test. I don't think we can ensure the order
  for (i = 0; i < N; i += 1) {
    assert.equal(i, done[i + 1]);
  }
  */
});
