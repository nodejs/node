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

const underscoreModules = [
  '_http_agent',
  '_http_client',
  '_http_common',
  '_http_incoming',
  '_http_outgoing',
  '_http_server',
  '_stream_duplex',
  '_stream_passthrough',
  '_stream_readable',
  '_stream_transform',
  '_stream_wrap',
  '_stream_writable',
  '_tls_common',
  '_tls_wrap',
  'node:_http_agent',
];

// Does not include modules starting with underscore
// (these can be required for historical reasons but
// are not proper documented public modules)
for (const module of underscoreModules) {
  assert(!isBuiltin(module));
  assert(!isBuiltin(`node:${module}`));
}
