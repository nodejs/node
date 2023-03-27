'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

// Test for "Breakpoint at specified location already exists" error.
const script = fixtures.path('debugger', 'three-lines.js');
const cli = startCLI(['--port=0', script]);

(async () => {
  try {
    await cli.waitForInitialBreak();
    await cli.waitForPrompt();
    await cli.command('setBreakpoint(1)');
    await cli.command('setBreakpoint(1)');
    await cli.waitFor(/Breakpoint at specified location already exists/);
  } finally {
    await cli.quit();
  }
})().then(common.mustCall());
