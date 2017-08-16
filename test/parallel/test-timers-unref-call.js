'use strict';

require('../common');

const Timer = process.binding('timer_wrap').Timer;
Timer.now = function() { return ++Timer.now.ticks; };
Timer.now.ticks = 0;

const t = setInterval(() => {}, 1);
const o = { _idleStart: 0, _idleTimeout: 1 };
t.unref.call(o);

setTimeout(clearInterval.bind(null, t), 2);
