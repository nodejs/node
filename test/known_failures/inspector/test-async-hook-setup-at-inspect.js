'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();
const helper = require('./inspector-helper');
const assert = require('assert');

const script = 'setInterval(() => { debugger; }, 50);';

helper.startNodeForInspectorTest(runTests, `--inspect=${common.PORT}`, script);

function runTests(harness) {
  helper.setupDebuggerAndAssertPausedMessage(harness, (msg) => {
    assert(
      !!msg.params.asyncStackTrace,
      `${Object.keys(msg.params)} contains "asyncStackTrace" property`);
  });
  harness.kill();
}
