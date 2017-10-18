'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();

const assert = require('assert');
const async_wrap = process.binding('async_wrap');
const { kTotals } = async_wrap.constants;
const inspector = require('inspector');

const setDepth = 'Debugger.setAsyncCallStackDepth';

function verifyAsyncHookDisabled(message) {
  assert.strictEqual(async_wrap.async_hook_fields[kTotals], 0);
}

function verifyAsyncHookEnabled(message) {
  assert.strictEqual(async_wrap.async_hook_fields[kTotals], 4);
}

// By default inspector async hooks should not have been installed.
verifyAsyncHookDisabled('inspector async hook should be disabled at startup');

const session = new inspector.Session();
verifyAsyncHookDisabled('creating a session should not enable async hooks');

session.connect();
verifyAsyncHookDisabled('connecting a session should not enable async hooks');

session.post('Debugger.enable', () => {
  verifyAsyncHookDisabled('enabling debugger should not enable async hooks');

  session.post(setDepth, { invalid: 'message' }, () => {
    verifyAsyncHookDisabled('invalid message should not enable async hooks');

    session.post(setDepth, { maxDepth: 'five' }, () => {
      verifyAsyncHookDisabled('invalid maxDepth (string) should not enable ' +
                              'async hooks');

      session.post(setDepth, { maxDepth: NaN }, () => {
        verifyAsyncHookDisabled('invalid maxDepth (NaN) should not enable ' +
                                'async hooks');

        session.post(setDepth, { maxDepth: 10 }, () => {
          verifyAsyncHookEnabled('valid message should enable async hooks');

          session.post(setDepth, { maxDepth: 0 }, () => {
            verifyAsyncHookDisabled('Setting maxDepth to 0 should disable ' +
                                    'async hooks');

            runTestSet2(session);
          });
        });
      });
    });
  });
});

function runTestSet2(session) {
  session.post(setDepth, { maxDepth: 32 }, () => {
    verifyAsyncHookEnabled('valid message should enable async hooks');

    session.post('Debugger.disable', () => {
      verifyAsyncHookDisabled('Debugger.disable should disable async hooks');

      session.post('Debugger.enable', () => {
        verifyAsyncHookDisabled('Enabling debugger should not enable hooks');

        session.post(setDepth, { maxDepth: 64 }, () => {
          verifyAsyncHookEnabled('valid message should enable async hooks');

          session.disconnect();
          verifyAsyncHookDisabled('Disconnecting session should disable ' +
                                  'async hooks');
        });
      });
    });
  });
}
