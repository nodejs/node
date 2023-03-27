'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const assert = require('assert');

// Debugger agent direct access.
{
  const cli = startCLI(['--port=0', fixtures.path('debugger/three-lines.js')]);
  const scriptPattern = /^\* (\d+): \S+debugger(?:\/|\\)three-lines\.js/m;

  async function testDebuggerLowLevel() {
    try {
      await cli.waitForInitialBreak();
      await cli.waitForPrompt();
      await cli.command('scripts');
      const [, scriptId] = cli.output.match(scriptPattern);
      await cli.command(
        `Debugger.getScriptSource({ scriptId: '${scriptId}' })`
      );
      assert.match(
        cli.output,
        /scriptSource:[ \n]*'(?:\(function \(|let x = 1)/);
      assert.match(
        cli.output,
        /let x = 1;/);
    } finally {
      await cli.quit();
    }
  }
  testDebuggerLowLevel();
}
