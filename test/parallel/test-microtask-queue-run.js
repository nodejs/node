'use strict';
require('../common');
const assert = require('assert');

function enqueueMicrotask(fn) {
  Promise.resolve().then(fn);
}

let done = 0;

process.on('exit', function() {
  assert.equal(done, 2);
});

// no nextTick, microtask
setTimeout(function() {
  enqueueMicrotask(function() {
    done++;
  });
}, 0);


// no nextTick, microtask with nextTick
setTimeout(function() {
  let called = false;

  enqueueMicrotask(function() {
    process.nextTick(function() {
      called = true;
    });
  });

  setTimeout(function() {
    if (called)
      done++;
  }, 0);

}, 0);
