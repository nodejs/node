var common = require('../common');
var assert = require('assert');

// Regression test GH-511: https://github.com/ry/node/issues/issue/511
// Make sure nextTick loops quickly

setTimeout(function () {
 t = Date.now() - t;
 STOP = 1;
 console.log(["ctr: ",ctr, ", t:", t, "ms -> ", (ctr/t).toFixed(2), "KHz"].join(''));
 assert.ok(ctr > 1000);
}, 2000);

var ctr = 0;
var STOP = 0;
var t = Date.now()+ 2;
while (t > Date.now()) ; //get in sync with clock

(function foo () {
  if (STOP) return;
  process.nextTick(foo);
  ctr++;
})();
