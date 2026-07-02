'use strict';
// Showcase: a node-api addon built with NODE_API_MODULE_USE_VTABLE that
// exercises the module v-table (Buffer APIs).
const common = require('../../common');
const assert = require('assert');

const binding = require(`./build/${common.buildType}/binding`);

// napi_create_buffer + napi_get_buffer_info round-trip through the v-table.
const buf = binding.createBuffer();
assert(Buffer.isBuffer(buf));
assert.strictEqual(binding.checkBuffer(buf), true);

// napi_create_buffer_copy likewise.
const copy = binding.copyBuffer();
assert(Buffer.isBuffer(copy));
assert.strictEqual(binding.checkBuffer(copy), true);

// napi_is_buffer distinguishes non-buffers and mismatched contents.
assert.strictEqual(binding.checkBuffer(Buffer.from('nope')), false);
assert.strictEqual(binding.checkBuffer({}), false);
