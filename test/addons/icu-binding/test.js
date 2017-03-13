'use strict';

const common = require('../../common');
if (!common.hasIntl) {
  common.skip('missing intl');
  process.exit(0);
}
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
assert.strictEqual(binding.cldrVersion, process.versions.cldr);
assert.strictEqual(binding.icuVersion, process.versions.icu);
