'use strict';
require('../common');
const assert = require('assert');
const { escapeArgument } = require('child_process');
assert.strictEqual(escapeArgument('foo'), 'foo');
assert.strictEqual(escapeArgument('foo bar'), "'foo bar'");
assert.strictEqual(escapeArgument('foo & bar'), "'foo & bar'");
assert.strictEqual(escapeArgument('foo & bar$'), "'foo & bar$'");
assert.strictEqual(escapeArgument('foo%bar'), 'foo%bar');
