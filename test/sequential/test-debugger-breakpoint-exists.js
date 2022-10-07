'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const script = fixtures.path('debugger', 'three-lines.js');
const cli = startCLI([script]);

// Test for "Breakpoint at specified location already exists" error.
(async () => {

  await cli.waitForInitialBreak();
  await cli.waitForPrompt()
  await cli.command('setBreakpoint(1)')
  await cli.command('setBreakpoint(1)')
  await cli.waitFor(/Breakpoint at specified location already exists/)
})()
  .finally(() => cli.quit())
