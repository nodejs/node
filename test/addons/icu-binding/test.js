'use strict';

const common = require('../../common');
if (!common.hasIntl) {
  common.skip('missing intl');
  process.exit(0);
}
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
assert.strictEqual(binding.icuVersion, process.versions.icu);
assert.strictEqual(binding.tzdataVersion, process.versions.tz);
