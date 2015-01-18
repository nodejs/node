require('../common');
var assert = require('assert');

var expected_keys = ['ares', 'http_parser', 'iojs', 'modules', 'node',
                     'openssl', 'uv', 'v8', 'zlib'];

assert.deepEqual(Object.keys(process.versions).sort(), expected_keys);
