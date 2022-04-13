'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const assert = require('assert');

// Debugger agent direct access.
{
  const cli = startCLI([fixtures.path('debugger/three-lines.js')]);
  const scriptPattern = /^\* (\d+): \S+debugger(?:\/|\\)three-lines\.js/m;

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('scripts'))
    .then(() => {
      const [, scriptId] = cli.output.match(scriptPattern);
      return cli.command(
        `Debugger.getScriptSource({ scriptId: '${scriptId}' })`
      );
    })
    .then(() => {
      assert.match(
        cli.output,
        /scriptSource:[ \n]*'(?:\(function \(|let x = 1)/);
      assert.match(
        cli.output,
        /let x = 1;/);
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
