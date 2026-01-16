'use strict';
// Addons: test_reference_double_free, test_reference_double_free_vtable

// This test makes no assertions. It tests a fix without which it will crash
// with a double free.

const { addonPath } = require('../../common/addon-test');
const addon = require(addonPath);

{ new addon.MyObject(true); }
{ new addon.MyObject(false); }
