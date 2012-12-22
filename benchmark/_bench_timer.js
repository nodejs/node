/*
 * This is a simple addition to allow for higher resolution timers.
 * It can be used to track time for both synchronous or asynchronous
 * calls. For synchronous calls pass a callback function like so:
 *
 *  var timer = require('./_bench_timer');
 *
 *  timer('myTest', function() {
 *    for (var i = 0; i < 1e6; i++)
 *      // ... run something here
 *    }
 *  });
 *
 * For asynchronous timers just pass the name. Then run it again with
 * the same name to finish it:
 *
 *  timer('checkAsync');
 *  setTimeout(function() {
 *    timer('checkAsync');
 *  }, 300);
 *
 * When this happens all currently queued benchmarks will be paused
 * until the asynchronous benchmark has completed.
 *
 * If the benchmark has been run with --expose_gc then the garbage
 * collector will be run between each test.
 *
 * The setTimeout delay can also be changed by passing a value to
 * timer.delay.
 */


var store = {};
var order = [];
var maxLength = 0;
var processing = false;
var asyncQueue = 0;
var GCd = typeof gc !== 'function' ? false : true;

function timer(name, fn) {
  if (maxLength < name.length)
    maxLength = name.length;
  if (!fn) {
    processing = false;
    if (!store[name]) {
      asyncQueue++;
      store[name] = process.hrtime();
      return;
    }
    displayTime(name, process.hrtime(store[name]));
    asyncQueue--;
  } else {
    store[name] = fn;
    order.push(name);
  }
  if (!processing && asyncQueue <= 0) {
    processing = true;
    setTimeout(run, timer.delay);
  }
}

timer.delay = 100;

function run() {
  if (asyncQueue > 0 || order.length <= 0)
    return;
  if (GCd) gc();
  setTimeout(function() {
    var name = order.shift();
    var fn = store[name];
    var ini = process.hrtime();
    fn();
    ini = process.hrtime(ini);
    displayTime(name, ini);
    run();
  }, timer.delay);
}

function displayTime(name, ini) {
  name += ': ';
  while (name.length < maxLength + 2)
    name += ' ';
  console.log(name + '%s \u00b5s',
              (~~((ini[0] * 1e6) + (ini[1] / 1e3)))
                .toString().replace(/(\d)(?=(\d\d\d)+(?!\d))/g, "$1,"));
}

module.exports = timer;
