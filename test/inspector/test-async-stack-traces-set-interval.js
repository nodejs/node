'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();
const helper = require('./inspector-helper');
const assert = require('assert');

const script = 'setInterval(() => { debugger; }, 50);';

helper.debugScriptAndAssertPausedMessage(script, (msg) => {
  assert(
    !!msg.params.asyncStackTrace,
    `${Object.keys(msg.params)} contains "asyncStackTrace" property`);
});
