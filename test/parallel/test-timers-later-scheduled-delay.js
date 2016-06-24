'use strict';

// See https://github.com/nodejs/node/issues/5426#issuecomment-228294811

const common = require('../common');
const TimerWrap = process.binding('timer_wrap').Timer;

setTimeout(common.mustCall(function first() {}), 1000);

setTimeout(function scheduleLast() {
  setTimeout(common.mustCall(function last() {
    const now = TimerWrap.now();

    if (now < 1500 - 200 || now > 1500 + 200) {
      common.fail('Delay for `last` is not within the margin of error.');
    }
  }), 1000);
}, 500);
