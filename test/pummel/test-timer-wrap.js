var common = require('../common');
var assert = require('assert');

var Timer = process.binding('timer_wrap').Timer;

var t = new Timer();

t.ontimeout = function() {
  console.log('timeout');
  t.close();
};

t.onclose = function() {
  console.log('close');
};

t.start(1000, 0);
