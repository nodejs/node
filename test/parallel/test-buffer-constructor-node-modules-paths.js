'use strict';

const child_process = require('child_process');
const assert = require('assert');
const common = require('../common');

if (process.env.NODE_PENDING_DEPRECATION)
  common.skip('test does not work when NODE_PENDING_DEPRECATION is set');

function test(main, callSite, expected) {
  const { stderr } = child_process.spawnSync(process.execPath, ['-p', `
  process.mainModule = { filename: ${JSON.stringify(main)} };

  vm.runInNewContext('new Buffer(10)', { Buffer }, {
    filename: ${JSON.stringify(callSite)}
  });`], { encoding: 'utf8' });
  if (expected)
    assert(stderr.includes('[DEP0005] DeprecationWarning'), stderr);
  else
    assert.strictEqual(stderr.trim(), '');
}

test('/a/node_modules/b.js', '/a/node_modules/x.js', false);
test('/a/node_modules/b.js', '/a/node_modules/foo/node_modules/x.js', false);
test('/a/node_modules/foo/node_modules/b.js', '/a/node_modules/x.js', false);
test('/node_modules/foo/b.js', '/node_modules/foo/node_modules/x.js', false);
test('/a.js', '/b.js', true);
test('/a.js', '/node_modules/b.js', false);
test('/node_modules/a.js.js', '/b.js', true);
test('c:\\a\\node_modules\\b.js', 'c:\\a\\node_modules\\x.js', false);
test('c:\\a\\node_modules\\b.js',
     'c:\\a\\node_modules\\foo\\node_modules\\x.js', false);
test('c:\\node_modules\\foo\\b.js',
     'c:\\node_modules\\foo\\node_modules\\x.js', false);
test('c:\\a.js', 'c:\\b.js', true);
test('c:\\a.js', 'c:\\node_modules\\b.js', false);
test('c:\\node_modules\\a.js', 'c:\\b.js', true);
