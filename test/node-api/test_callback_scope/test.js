'use strict';

const common = require('../../common');
const assert = require('assert');
const { runInCallbackScope } = require(`./build/${common.buildType}/binding`);

assert.strictEqual(runInCallbackScope({}, 'test-resource', () => 42), 42);

{
  process.once('uncaughtException', common.mustCall((err) => {
    assert.strictEqual(err.message, 'foo');
  }));

  runInCallbackScope({}, 'test-resource', () => {
    throw new Error('foo');
  });
}
