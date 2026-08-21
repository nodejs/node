// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { getOptionValue, parseNodeOptionsEnvVar } = require('internal/options');

Map.prototype.get =
  common.mustNotCall('`getOptionValue` must not call user-mutable method');
assert.strictEqual(getOptionValue('--expose-internals'), true);

Object.prototype['--nonexistent-option'] = 'foo';
assert.strictEqual(getOptionValue('--nonexistent-option'), undefined);

// Make the test common global leak test happy.
delete Object.prototype['--nonexistent-option'];

// parseNodeOptionsEnvVar tokenizes a NODE_OPTIONS-style string.
assert.deepStrictEqual(
  parseNodeOptionsEnvVar('--max-old-space-size=4096 --no-warnings'),
  ['--max-old-space-size=4096', '--no-warnings']
);

// Quoted strings are unquoted during parsing.
assert.deepStrictEqual(
  parseNodeOptionsEnvVar('--require "file with spaces.js"'),
  ['--require', 'file with spaces.js']
);

// Empty string returns an empty array.
assert.deepStrictEqual(parseNodeOptionsEnvVar(''), []);

// Throws on unterminated string.
assert.throws(
  () => parseNodeOptionsEnvVar('--require "unterminated'),
  { name: 'Error', message: /unterminated string/ }
);

// Throws on invalid escape at end of string.
assert.throws(
  () => parseNodeOptionsEnvVar('--require "foo\\'),
  { name: 'Error', message: /invalid escape/ }
);
