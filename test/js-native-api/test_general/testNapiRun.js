'use strict';
// Addons: test_general, test_general_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');

// `addon` is referenced through the eval expression in testFile
const addon = require(addonPath);

const testCase = '(41.92 + 0.08);';
const expected = 42;
const actual = addon.testNapiRun(testCase);

assert.strictEqual(actual, expected);
assert.throws(() => addon.testNapiRun({ abc: 'def' }), /string was expected/);
