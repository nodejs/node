var common = require('../common');
var assert = require('assert');

var DELAY = 50;
var WINDOW = 5;

function nearly(a, b) {
  if (Math.abs(a-b) > WINDOW)
    assert.strictEqual(a, b);
}

function work(ms) {
  var start = Date.now();
  while (Date.now() - start < ms);
}

function test() {
  var start = Date.now();
  setTimeout(function() {
    work(DELAY + 20);
    setTimeout(function() {
      nearly(DELAY*3+20, Date.now() - start);
    }, DELAY);
  }, DELAY);
}

test();
