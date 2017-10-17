'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();

const assert = require('assert');
const async_hooks = require('async_hooks');
const inspector = require('inspector');

// Verify that inspector-async-hook is not registered,
// thus emitInit() ignores invalid arguments
// See test/async-hooks/test-emit-init.js
function verifyAsyncHookDisabled(message) {
  assert.doesNotThrow(() => async_hooks.emitInit(), message);
}

// Verify that inspector-async-hook is registered
// by checking that emitInit with invalid arguments
// throw an error.
// See test/async-hooks/test-emit-init.js
function verifyAsyncHookEnabled(message) {
  assert.throws(() => async_hooks.emitInit(), message);
}

// By default inspector async hooks should not have been installed.
verifyAsyncHookDisabled('inspector async hook should be disabled at startup');

const session = new inspector.Session();
verifyAsyncHookDisabled('creating a session should not enable async hooks');

session.connect();
verifyAsyncHookDisabled('creating a session should not enable async hooks');

session.post('Debugger.setAsyncCallStackDepth', { invalid: 'message' }, () => {
  verifyAsyncHookDisabled('invalid message should not enable async hooks');

  session.post('Debugger.setAsyncCallStackDepth', { maxDepth: 'five' }, () => {
    verifyAsyncHookDisabled('invalid maxDepth (sting) should not enable async' +
      ' hooks');

    session.post('Debugger.setAsyncCallStackDepth', { maxDepth: NaN }, () => {
      verifyAsyncHookDisabled('invalid maxDepth (NaN) should not enable async' +
        ' hooks');

      session.post('Debugger.setAsyncCallStackDepth', { maxDepth: 10 }, () => {
        verifyAsyncHookEnabled('valid message should enable async hook');

        session.post('Debugger.setAsyncCallStackDepth', { maxDepth: 0 }, () => {
          verifyAsyncHookDisabled('Setting maxDepth to 0 should disable ' +
            'async hooks');
        });
      });
    });
  });
});
