'use strict';
// Showcase: a js-native-api addon built with NODE_API_MODULE_USE_VTABLE that
// exercises the JS v-table (string and number interop).
const common = require('../../common');
const assert = require('assert');

const binding = require(`./build/${common.buildType}/binding`);

// napi_create_string_utf8 through the v-table.
assert.strictEqual(binding.hello(), 'world');

// napi_get_value_double + napi_create_double round-trip through the v-table.
assert.strictEqual(binding.double(21), 42);
assert.strictEqual(binding.double(-1.5), -3);
