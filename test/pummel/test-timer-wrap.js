'use strict';
const common = require('../common');

const Timer = process.binding('timer_wrap').Timer;
const kOnTimeout = Timer.kOnTimeout;

const t = new Timer();

t.start(1000);

t[kOnTimeout] = common.mustCall(function() {
  console.log('timeout');
  t.close();
});
