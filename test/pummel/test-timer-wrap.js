'use strict';
var common = require('../common');
var assert = require('assert');

var timeouts = 0;
var Timer = process.binding('timer_wrap').Timer;
var kOnTimeout = Timer.kOnTimeout;

var t = new Timer();

t.start(1000, 0);

t[kOnTimeout] = function() {
  timeouts++;
  console.log('timeout');
  t.close();
};

process.on('exit', function() {
  assert.equal(1, timeouts);
});
