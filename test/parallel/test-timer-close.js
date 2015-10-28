'use strict';
// Flags: --expose_internals
require('../common');
var assert = require('assert');

var t = new (require('binding/timer_wrap').Timer);
var called = 0;
function onclose() {
  called++;
}

t.close(onclose);
t.close(onclose);

process.on('exit', function() {
  assert.equal(1, called);
});
