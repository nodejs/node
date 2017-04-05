// Flags: --icu-data-dir=test/fixtures/empty/
'use strict';
const common = require('../common');
const assert = require('assert');
const config = process.binding('config');

if (!(common.hasIntl && common.hasSmallICU)) {
  common.skip('not (Intl AND SmallICU)');
  return;
}
assert.deepStrictEqual(Intl.NumberFormat.supportedLocalesOf('en'), []);
assert.strictEqual(config.icuDataDir, 'test/fixtures/empty/');
