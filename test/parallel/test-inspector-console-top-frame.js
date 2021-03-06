'use strict';

// Verify that the line containing console.log is reported as a top stack frame
// of the consoleAPICalled notification.
// Changing this will break many Inspector protocol clients, including
// debuggers that use that value for navigating from console messages to code.

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { Session } = require('inspector');
const { basename } = require('path');

function logMessage() {
  console.log('Log a message');
}

const session = new Session();
let topFrame;
session.once('Runtime.consoleAPICalled', (notification) => {
  topFrame = (notification.params.stackTrace.callFrames[0]);
});
session.connect();
session.post('Runtime.enable');

logMessage();  // Triggers Inspector notification

session.disconnect();
assert.strictEqual(basename(topFrame.url), basename(__filename));
assert.strictEqual(topFrame.lineNumber, 15);
