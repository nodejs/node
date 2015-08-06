'use strict';
const common = require('../common');
var fixturesDir = common.fixturesDir;
const assert = require('assert');
const path = require('path');

assert.equal(
    path.join(__dirname, '../fixtures/a.js').toLowerCase(),
    require.resolve('../fixtures/a').toLowerCase());
assert.equal(
    path.join(fixturesDir, 'a.js').toLowerCase(),
    require.resolve(path.join(fixturesDir, 'a')).toLowerCase());
assert.equal(
    path.join(fixturesDir, 'nested-index', 'one', 'index.js').toLowerCase(),
    require.resolve('../fixtures/nested-index/one').toLowerCase());
assert.equal('path', require.resolve('path'));

console.log('ok');
