var common = require('../common');
var assert = require('assert');

var timeouts = 0;
var Timer = process.binding('timer_wrap').Timer;

var t = new Timer();

t.start(1000, 0);

t.ontimeout = function() {
  timeouts++;
  console.log('timeout');
  t.close();
};

process.on('exit', function() {
  assert.equal(1, timeouts);
});
