'use strict';
// Addons: test_reference_double_free, test_reference_double_free_vtable

// This test makes no assertions. It tests that calling napi_remove_wrap and
// napi_delete_reference consecutively doesn't crash the process.

const { addonPath } = require('../../common/addon-test');

const addon = require(addonPath);

addon.deleteImmediately({});
