'use strict';
require('../common');

var Timer = process.binding('timer_wrap').Timer;
Timer.now = function() { return ++Timer.now.ticks; };
Timer.now.ticks = 0;

var t = setInterval(function() {}, 1);
var o = { _idleStart: 0, _idleTimeout: 1 };
t.unref.call(o);

setTimeout(clearInterval.bind(null, t), 2);
