'use strict';

const common = require('../common');
const assert = require('assert');

// We don't really care about the calling results here.
// So, this makes the test less fragile.
const noop = () => {};

const t1 = setTimeout(common.mustNotCall(), 1);
const t2 = setTimeout(common.mustCall(), 1);
const i1 = setInterval(common.mustNotCall(), 1);
const i2 = setInterval(noop, 1);
i2.unref();

// Keep process alive for i2 to call once due to timer ordering.
setTimeout(common.mustCall(), 1);

clearTimeout(t1);
clearInterval(i1);

process.on('exit', () => {
  assert.strictEqual(t1._destroyed, true);
  assert.strictEqual(t2._destroyed, true);
  assert.strictEqual(i1._destroyed, true);
  assert.strictEqual(i2._destroyed, false);
});
