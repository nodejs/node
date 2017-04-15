'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const helper = require('./inspector-helper.js');

function shouldShutDown(session) {
  session
    .sendInspectorCommands([
        { 'method': 'Debugger.enable' },
        { 'method': 'Debugger.pause' },
    ])
    .disconnect(true);
}

function runTests(harness) {
  // 1 second wait to make sure the inferior began running the script
  setTimeout(() => harness.runFrontendSession([shouldShutDown]).kill(), 1000);
}

const script = 'setInterval(() => {debugger;}, 60000);';
helper.startNodeForInspectorTest(runTests, '--inspect', script);
