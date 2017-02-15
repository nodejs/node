'use strict';
const { test } = require('tap');

const startCLI = require('./start-cli');

test('Debugger agent direct access', (t) => {
  const cli = startCLI(['examples/empty.js']);
  const scriptPattern = /^\* (\d+): examples(?:\/|\\)empty.js/;

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('scripts'))
    .then(() => {
      const [, scriptId] = cli.output.match(scriptPattern);
      return cli.command(
        `Debugger.getScriptSource({ scriptId: '${scriptId}' })`
      );
    })
    .then(() => {
      t.match(
        cli.output,
        /scriptSource: '\(function \([^)]+\) \{ \\n}\);'/);
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
