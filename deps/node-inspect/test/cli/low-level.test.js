'use strict';
const { test } = require('tap');

const startCLI = require('./start-cli');

test('Debugger agent direct access', (t) => {
  const cli = startCLI(['examples/three-lines.js']);
  const scriptPattern = /^\* (\d+): examples(?:\/|\\)three-lines.js/;

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
      t.match(
        cli.output,
        /scriptSource:[ \n]*'(?:\(function \(|let x = 1)/);
      t.match(
        cli.output,
        /let x = 1;/);
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
