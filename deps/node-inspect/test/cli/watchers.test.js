'use strict';
const { test } = require('tap');

const startCLI = require('./start-cli');

test('stepping through breakpoints', (t) => {
  const cli = startCLI(['examples/break.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
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
      t.match(cli.output, 'x is not defined');
    })
    .then(() => cli.command('unwatch("42")'))
    .then(() => cli.stepCommand('n'))
    .then(() => {
      t.match(cli.output, '0: x = 10');
      t.match(cli.output, '1: "Hello" = \'Hello\'');
      t.match(cli.output, '2: NaN = NaN');
      t.match(cli.output, '3: true = true');
      t.match(cli.output, '4: [1, 2] = [ 1, 2 ]');
      t.match(
        cli.output,
        /5: process\.env =\n\s+\{[\s\S]+,\n\s+\.\.\. \}/,
        'shows "..." for process.env');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
