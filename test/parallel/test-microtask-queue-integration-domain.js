'use strict';
var common = require('../common');
var assert = require('assert');
var domain = require('domain');

var implementations = [
  function(fn) {
    Promise.resolve().then(fn);
  },
  function(fn) {
    var obj = {};

    Object.observe(obj, fn);

    obj.a = 1;
  }
];

var expected = 0;
var done = 0;

process.on('exit', function() {
  assert.equal(done, expected);
});

function test(scheduleMicrotask) {
  var nextTickCalled = false;
  expected++;

  scheduleMicrotask(function() {
    process.nextTick(function() {
      nextTickCalled = true;
    });

    setTimeout(function() {
      assert(nextTickCalled);
      done++;
    }, 0);
  });
}

// first tick case
implementations.forEach(test);

// tick callback case
setTimeout(function() {
  implementations.forEach(function(impl) {
    process.nextTick(test.bind(null, impl));
  });
}, 0);
