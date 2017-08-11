'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const helper = require('./inspector-helper.js');

function test(session) {
  session.sendInspectorCommands([
    { 'method': 'Runtime.runIfWaitingForDebugger' },
    { 'method': 'Profiler.setSamplingInterval', 'params': { 'interval': 100 } },
    { 'method': 'Profiler.enable' },
    { 'method': 'Profiler.start' }]);
  session.expectStderrOutput('Waiting for the debugger to disconnect...');
  session.sendInspectorCommands({ 'method': 'Profiler.stop' });
  session.disconnect(true);
}

function runTests(harness) {
  harness.runFrontendSession([test]).expectShutDown(0);
}

helper.startNodeForInspectorTest(runTests, ['--inspect-brk'], 'let a = 2;');
