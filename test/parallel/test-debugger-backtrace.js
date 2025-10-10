'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

// Display and navigate backtrace.
{
  const scriptFullPath = fixtures.path('debugger', 'backtrace.js');
  const script = path.relative(process.cwd(), scriptFullPath);
  const cli = startCLI([script]);

  async function runTest() {
    try {
      await cli.waitForInitialBreak();
      await cli.waitForPrompt();
      await cli.stepCommand('c');
      await cli.command('bt');
      assert.ok(cli.output.includes(`#0 topFn ${script}:7:2`));
      await cli.command('backtrace');
      assert.ok(cli.output.includes(`#0 topFn ${script}:7:2`));
    } finally {
      await cli.quit();
    }
  }

  runTest();
}
