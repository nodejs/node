var common = require('../common');
var fixturesDir = common.fixturesDir;
var assert = require('assert');
var path = require('path');

assert.equal(path.join(__dirname, '../fixtures/a.js'),
             require.resolve('../fixtures/a'));
assert.equal(path.join(fixturesDir, 'a.js'),
             require.resolve(path.join(fixturesDir, 'a')));
assert.equal(path.join(fixturesDir, 'nested-index', 'one', 'index.js'),
             require.resolve('../fixtures/nested-index/one'));
assert.equal('path', require.resolve('path'));

console.log('ok');
