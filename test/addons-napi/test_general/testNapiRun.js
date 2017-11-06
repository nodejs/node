'use strict';

const common = require('../../common');
const assert = require('assert');

// addon is referenced through the eval expression in testFile
// eslint-disable-next-line no-unused-vars
const addon = require(`./build/${common.buildType}/test_general`);

const args = [41.92, 0.08];
const testCase = '(' + args.join(' + ') + ');';
const expected = 43;
const actual = addon.testNapiRun(testCase);

assert.strictEqual(actual, expected, `testNapiRun${testCase} gets ${actual} not expected ${expected}`);
assert.throws(() => addon.testNapiRun({abc: 'def'}), /string was expected/);