'use strict';

const common = require('../../common');
const assert = require('assert');

// addon is referenced through the eval expression in testFile
// eslint-disable-next-line no-unused-vars
const addon = require(`./build/${common.buildType}/test_general`);

assert.strictEqual(addon.testNapiRun('(41.92 + 0.08);'), 42,
                   'napi_run_script() works correctly');
assert.throws(() => addon.testNapiRun({ abc: 'def' }), /string was expected/);
