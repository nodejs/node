'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const helper = require('./inspector-helper.js');

function testStop(harness) {
  harness.expectShutDown(42);
}

helper.startNodeForInspectorTest(testStop, '--inspect',
                                 'process._debugEnd();process.exit(42);');
