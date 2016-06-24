'use strict';

// See https://github.com/nodejs/node/issues/5426

const common = require('../common');
const TimerWrap = process.binding('timer_wrap').Timer;

function sleep(duration) {
  var start = Date.now();
  while ((Date.now() - start) < duration) {
    for (var i = 0; i < 1e5;) i++;
  }
}

var last = TimerWrap.now();
var count = 0;

const interval = setInterval(common.mustCall(repeat, 4), 500);

function repeat() {
  if (++count === 4) {
    clearInterval(interval);
  }
  const now = TimerWrap.now();

  if (now < last + 500 - 200 || now > last + 500 + 200) {
    common.fail(`\`repeat\` delay: ${count} (now: ${now}, last: ${last})\n` +
                'is not within the margin of error.');
  }
  last = now;
  sleep(500);
}
