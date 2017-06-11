'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const helper = require('./inspector-helper.js');

function testResume(session) {
  session.sendCommandsAndExpectClose([
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ]);
}

function testDisconnectSession(harness) {
  harness
    .runFrontendSession([
      testResume,
    ]).expectShutDown(42);
}

const script = 'process._debugEnd();' +
               'process._debugProcess(process.pid);' +
               'setTimeout(() => {console.log("Done");process.exit(42)});';

helper.startNodeForInspectorTest(testDisconnectSession, '--inspect-brk',
                                 script);
