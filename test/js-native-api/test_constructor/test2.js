'use strict';
// Addons: test_constructor, test_constructor_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');

// Testing api calls for a constructor that defines properties
const TestConstructor =
    require(addonPath).constructorName;
assert.strictEqual(TestConstructor.name, 'MyObject');
