'use strict';

const common = require('../../common');
const assert = require('assert');

function testUncaughtException(binding) {
  const callbackCheck = common.mustCall((err) => {
    assert.throws(() => { throw err; }, /callback error/);
    process.removeListener('uncaughtException', callbackCheck);
    process.on('uncaughtException', finalizerCheck);
  });
  const finalizerCheck = common.mustCall((err) => {
    assert.throws(() => { throw err; }, /finalizer error/);
  });
  process.on('uncaughtException', callbackCheck);

  binding.CallIntoModule(
    common.mustCall(() => {
      throw new Error('callback error');
    }),
    {},
    'resource_name',
    common.mustCall(function finalizer() {
      throw new Error('finalizer error');
    }),
  );
}

module.exports = {
  testUncaughtException,
};
