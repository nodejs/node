'use strict';
// Addons: binding, binding_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const { runInCallbackScope } = require(addonPath);

assert.strictEqual(runInCallbackScope({}, 'test-resource', () => 42), 42);

{
  process.once('uncaughtException', common.mustCall((err) => {
    assert.strictEqual(err.message, 'foo');
  }));

  runInCallbackScope({}, 'test-resource', () => {
    throw new Error('foo');
  });
}
