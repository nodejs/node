'use strict';
const common = require('../common');

var Timer = process.binding('timer_wrap').Timer;
var kOnTimeout = Timer.kOnTimeout;

var t = new Timer();

t.start(1000);

t[kOnTimeout] = common.mustCall(function() {
  console.log('timeout');
  t.close();
});
