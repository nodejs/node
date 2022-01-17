'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

// Test for "Breakpoint at specified location already exists" error.
{
  const script = fixtures.path('debugger', 'three-lines.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('setBreakpoint(1)'))
    .then(() => cli.command('setBreakpoint(1)'))
    .then(() => cli.waitFor(/Breakpoint at specified location already exists/))
    .then(() => cli.quit())
    .then(null, onFatal);
}
