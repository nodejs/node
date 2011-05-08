var common = require('../common');
var assert = require('assert');

var t = Date.now();
var diff;
setTimeout(function () {
  diff = Date.now() - t;
  console.error(diff);
}, 0.1);


process.on('exit', function() {
  assert.ok(diff < 100);
});
