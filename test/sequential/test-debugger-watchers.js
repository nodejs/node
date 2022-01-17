'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

// Stepping through breakpoints.
{
  const cli = startCLI([fixtures.path('debugger/break.js')]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('watch("x")'))
    .then(() => cli.command('watch("\\"Hello\\"")'))
    .then(() => cli.command('watch("42")'))
    .then(() => cli.command('watch("NaN")'))
    .then(() => cli.command('watch("true")'))
    .then(() => cli.command('watch("[1, 2]")'))
    .then(() => cli.command('watch("process.env")'))
    .then(() => cli.command('watchers'))
    .then(() => {
      assert.match(cli.output, /x is not defined/);
    })
    .then(() => cli.command('unwatch("42")'))
    .then(() => cli.stepCommand('n'))
    .then(() => {
      assert.match(cli.output, /0: x = 10/);
      assert.match(cli.output, /1: "Hello" = 'Hello'/);
      assert.match(cli.output, /2: NaN = NaN/);
      assert.match(cli.output, /3: true = true/);
      assert.match(cli.output, /4: \[1, 2\] = \[ 1, 2 \]/);
      assert.match(
        cli.output,
        /5: process\.env =\n\s+\{[\s\S]+,\n\s+\.\.\. \}/,
        'shows "..." for process.env');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
