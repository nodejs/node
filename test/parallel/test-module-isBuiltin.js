'use strict';
require('../common');
const assert = require('assert');
const { isBuiltin } = require('module');

// Includes modules in lib/ (even deprecated ones)
assert(isBuiltin('http'));
assert(isBuiltin('sys'));
assert(isBuiltin('node:fs'));
assert(isBuiltin('node:test'));

// Does not include internal modules
assert(!isBuiltin('internal/errors'));
assert(!isBuiltin('test'));
assert(!isBuiltin(''));
assert(!isBuiltin(undefined));

// Does not include modules starting with underscore
// (these can be required for historical reasons but
// are not proper documented public modules)
assert(!isBuiltin('_http_agent'));
assert(!isBuiltin('_http_client'));
assert(!isBuiltin('_http_common'));
assert(!isBuiltin('_http_incoming'));
assert(!isBuiltin('_http_outgoing'));
assert(!isBuiltin('_http_server'));
assert(!isBuiltin('_stream_duplex'));
assert(!isBuiltin('_stream_passthrough'));
assert(!isBuiltin('_stream_readable'));
assert(!isBuiltin('_stream_transform'));
assert(!isBuiltin('_stream_wrap'));
assert(!isBuiltin('_stream_writable'));
assert(!isBuiltin('_tls_common'));
assert(!isBuiltin('_tls_wrap'));
