'use strict';
require('../common');
const assert = require('assert');

const t = new (process.binding('timer_wrap').Timer)();
let called = 0;
function onclose() {
  called++;
}

t.close(onclose);
t.close(onclose);

process.on('exit', function() {
  assert.strictEqual(1, called);
});
