'use strict';
const common = require('../common');
const assert = require('assert');

const timer = setTimeout(common.mustNotCall(), 10);
const interval = setInterval(common.mustNotCall(), 10);
const immediate = setImmediate(common.mustNotCall());

timer[Symbol.dispose]();
interval[Symbol.dispose]();
immediate[Symbol.dispose]();


process.on('exit', () => {
  assert.strictEqual(timer._destroyed, true);
  assert.strictEqual(interval._destroyed, true);
  assert.strictEqual(immediate._destroyed, true);
});
