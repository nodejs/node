'use strict';
const common = require('../common');
var assert = require('assert');

var WINDOW = 200; // why is does this need to be so big?

var interval_count = 0;

// check that these don't blow up.
clearTimeout(null);
clearInterval(null);

assert.equal(true, setTimeout instanceof Function);
var starttime = new Date();
setTimeout(common.mustCall(function() {
  var endtime = new Date();

  var diff = endtime - starttime;
  assert.ok(diff > 0);
  console.error('diff: ' + diff);

  assert.equal(true, 1000 - WINDOW < diff && diff < 1000 + WINDOW);
}), 1000);

// this timer shouldn't execute
var id = setTimeout(function() { assert.equal(true, false); }, 500);
clearTimeout(id);

setInterval(function() {
  interval_count += 1;
  var endtime = new Date();

  var diff = endtime - starttime;
  assert.ok(diff > 0);
  console.error('diff: ' + diff);

  var t = interval_count * 1000;

  assert.equal(true, t - WINDOW < diff && diff < t + WINDOW);

  assert.equal(true, interval_count <= 3);
  if (interval_count === 3)
    clearInterval(this);
}, 1000);


// Single param:
setTimeout(function(param) {
  assert.equal('test param', param);
}, 1000, 'test param');

var interval_count2 = 0;
setInterval(function(param) {
  ++interval_count2;
  assert.equal('test param', param);

  if (interval_count2 === 3)
    clearInterval(this);
}, 1000, 'test param');


// Multiple param
setTimeout(function(param1, param2) {
  assert.equal('param1', param1);
  assert.equal('param2', param2);
}, 1000, 'param1', 'param2');

var interval_count3 = 0;
setInterval(function(param1, param2) {
  ++interval_count3;
  assert.equal('param1', param1);
  assert.equal('param2', param2);

  if (interval_count3 === 3)
    clearInterval(this);
}, 1000, 'param1', 'param2');

// setInterval(cb, 0) should be called multiple times.
var count4 = 0;
var interval4 = setInterval(function() {
  if (++count4 > 10) clearInterval(interval4);
}, 0);


// we should be able to clearTimeout multiple times without breakage.
var expectedTimeouts = 3;

function t() {
  expectedTimeouts--;
}

setTimeout(t, 200);
setTimeout(t, 200);
var y = setTimeout(t, 200);

clearTimeout(y);
setTimeout(t, 200);
clearTimeout(y);


process.on('exit', function() {
  assert.equal(3, interval_count);
  assert.equal(11, count4);
  assert.equal(0, expectedTimeouts, 'clearTimeout cleared too many timeouts');
});
