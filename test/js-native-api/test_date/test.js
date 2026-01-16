'use strict';
// Addons: test_date, test_date_vtable

const { addonPath } = require('../../common/addon-test');

// This tests the date-related Node-API calls

const assert = require('assert');
const test_date = require(addonPath);

const dateTypeTestDate = test_date.createDate(1549183351);
assert.strictEqual(test_date.isDate(dateTypeTestDate), true);

assert.strictEqual(test_date.isDate(new Date(1549183351)), true);

assert.strictEqual(test_date.isDate(2.4), false);
assert.strictEqual(test_date.isDate('not a date'), false);
assert.strictEqual(test_date.isDate(undefined), false);
assert.strictEqual(test_date.isDate(null), false);
assert.strictEqual(test_date.isDate({}), false);

assert.strictEqual(test_date.getDateValue(new Date(1549183351)), 1549183351);
