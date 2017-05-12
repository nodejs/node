'use strict';
const common = require('../../common');
const assert = require('assert');

const test_globals = require(`./build/${common.buildType}/test_globals`);

assert.strictEqual(test_globals.getUndefined(), undefined);
assert.strictEqual(test_globals.getNull(), null);
