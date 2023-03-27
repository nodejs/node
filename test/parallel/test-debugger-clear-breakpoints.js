'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

// clearBreakpoint
{
  const scriptFullPath = fixtures.path('debugger', 'break.js');
  const script = path.relative(process.cwd(), scriptFullPath);
  const cli = startCLI(['--port=0', script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('sb("break.js", 3)'))
    .then(() => cli.command('sb("break.js", 9)'))
    .then(() => cli.command('breakpoints'))
    .then(() => {
      assert.ok(cli.output.includes(`#0 ${script}:3`));
      assert.ok(cli.output.includes(`#1 ${script}:9`));
    })
    .then(() => cli.command('clearBreakpoint("break.js", 4)'))
    .then(() => {
      assert.match(cli.output, /Could not find breakpoint/);
    })
    .then(() => cli.command('clearBreakpoint("not-such-script.js", 3)'))
    .then(() => {
      assert.match(cli.output, /Could not find breakpoint/);
    })
    .then(() => cli.command('clearBreakpoint("break.js", 3)'))
    .then(() => cli.command('breakpoints'))
    .then(() => {
      assert.ok(cli.output.includes(`#0 ${script}:9`));
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      assert.ok(
        cli.output.includes(`break in ${script}:9`),
        'hits the 2nd breakpoint because the 1st was cleared');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
