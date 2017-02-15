'use strict';
const common = require('../common');
const fixturesDir = common.fixturesDir;
const assert = require('assert');
const path = require('path');

assert.strictEqual(
    path.join(__dirname, '../fixtures/a.js').toLowerCase(),
    require.resolve('../fixtures/a').toLowerCase());
assert.strictEqual(
    path.join(fixturesDir, 'a.js').toLowerCase(),
    require.resolve(path.join(fixturesDir, 'a')).toLowerCase());
assert.strictEqual(
    path.join(fixturesDir, 'nested-index', 'one', 'index.js').toLowerCase(),
    require.resolve('../fixtures/nested-index/one').toLowerCase());
assert.strictEqual('path', require.resolve('path'));

console.log('ok');
