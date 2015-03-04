var common = require('../common');

var Timer = process.binding('timer_wrap').Timer;

var t = setInterval(function() {}, 1);
var o = { _idleStart: 0, _idleTimeout: 1 };
t.unref.call(o);

setTimeout(clearInterval.bind(null, t), 2);
