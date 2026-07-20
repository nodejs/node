'use strict';
const common = require('../common');
const assert = require('assert');

common.expectWarning({
  DeprecationWarning: {
    DEP0208: 'Timeout.prototype[Symbol.dispose] is deprecated. Use clearTimeout instead.',
  },
});

const timer = setTimeout(common.mustNotCall(), 10);
const interval = setInterval(common.mustNotCall(), 10);
const immediate = setImmediate(common.mustNotCall());

timer[Symbol.dispose]();
// Second call should not emit another warning (codes are warned once).
interval[Symbol.dispose]();
// Immediate is not deprecated.
immediate[Symbol.dispose]();

process.on('exit', () => {
  assert.strictEqual(timer._destroyed, true);
  assert.strictEqual(interval._destroyed, true);
  assert.strictEqual(immediate._destroyed, true);
});
