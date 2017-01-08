'use strict';
require('../common');
const assert = require('assert');

let exceptions = 0;
let timer1 = 0;
let timer2 = 0;

// the first timer throws...
console.error('set first timer');
setTimeout(function() {
  console.error('first timer');
  timer1++;
  throw new Error('BAM!');
}, 100);

// ...but the second one should still run
console.error('set second timer');
setTimeout(function() {
  console.error('second timer');
  assert.strictEqual(timer1, 1);
  timer2++;
}, 100);

function uncaughtException(err) {
  console.error('uncaught handler');
  assert.strictEqual(err.message, 'BAM!');
  exceptions++;
}
process.on('uncaughtException', uncaughtException);

let exited = false;
process.on('exit', function() {
  assert(!exited);
  exited = true;
  process.removeListener('uncaughtException', uncaughtException);
  assert.strictEqual(exceptions, 1);
  assert.strictEqual(timer1, 1);
  assert.strictEqual(timer2, 1);
});
