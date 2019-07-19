// Flags: --experimental-exports
'use strict';

require('../common');

const assert = require('assert');
const { createRequire } = require('module');
const path = require('path');

const fixtureRequire =
  createRequire(path.resolve(__dirname, '../fixtures/imaginary.js'));

assert.strictEqual(fixtureRequire('pkgexports/valid-cjs'), 'asdf');

assert.strictEqual(fixtureRequire('baz/index'), 'eye catcher');

assert.strictEqual(fixtureRequire('pkgexports/sub/asdf.js'), 'asdf');

assert.strictEqual(fixtureRequire('pkgexports/space'), 'encoded path');

assert.throws(
  () => fixtureRequire('pkgexports/not-a-known-entry'),
  (e) => {
    assert.strictEqual(e.code, 'ERR_PATH_NOT_EXPORTED');
    return true;
  });

assert.throws(
  () => fixtureRequire('pkgexports-number/hidden.js'),
  (e) => {
    assert.strictEqual(e.code, 'ERR_PATH_NOT_EXPORTED');
    return true;
  });

assert.throws(
  () => fixtureRequire('pkgexports/sub/not-a-file.js'),
  (e) => {
    assert.strictEqual(e.code, 'MODULE_NOT_FOUND');
    return true;
  });

assert.throws(
  () => fixtureRequire('pkgexports/sub/./../asdf.js'),
  (e) => {
    assert.strictEqual(e.code, 'ERR_PATH_NOT_EXPORTED');
    return true;
  });
