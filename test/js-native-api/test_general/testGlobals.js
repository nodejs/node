'use strict';
// Addons: test_general, test_general_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');

const test_globals = require(addonPath);

assert.strictEqual(test_globals.getUndefined(), undefined);
assert.strictEqual(test_globals.getNull(), null);
