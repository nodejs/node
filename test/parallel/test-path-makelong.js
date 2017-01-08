'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');

if (common.isWindows) {
  const file = path.join(common.fixturesDir, 'a.js');
  const resolvedFile = path.resolve(file);

  assert.strictEqual('\\\\?\\' + resolvedFile, path._makeLong(file));
  assert.strictEqual('\\\\?\\' + resolvedFile, path._makeLong('\\\\?\\' +
                     file));
  assert.strictEqual('\\\\?\\UNC\\someserver\\someshare\\somefile',
                     path._makeLong('\\\\someserver\\someshare\\somefile'));
  assert.strictEqual('\\\\?\\UNC\\someserver\\someshare\\somefile', path
    ._makeLong('\\\\?\\UNC\\someserver\\someshare\\somefile'));
  assert.strictEqual('\\\\.\\pipe\\somepipe',
                     path._makeLong('\\\\.\\pipe\\somepipe'));
}

assert.strictEqual(path._makeLong(null), null);
assert.strictEqual(path._makeLong(100), 100);
assert.strictEqual(path._makeLong(path), path);
assert.strictEqual(path._makeLong(false), false);
assert.strictEqual(path._makeLong(true), true);
