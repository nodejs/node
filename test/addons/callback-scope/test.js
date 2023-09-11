'use strict';

const common = require('../../common');
const assert = require('assert');
const { runInCallbackScope } = require(`./build/${common.buildType}/binding`);

assert.strictEqual(runInCallbackScope({}, 0, 0, () => 42), 42);

{
  process.once('uncaughtException', common.mustCall((err) => {
    assert.strictEqual(err.message, 'foo');
  }));

  runInCallbackScope({}, 0, 0, () => {
    throw new Error('foo');
  });
}
