'use strict';
const Path = require('path');

const { test } = require('tap');

const startCLI = require('./start-cli');

test('display and navigate backtrace', (t) => {
  const script = Path.join('examples', 'backtrace.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.stepCommand('c'))
    .then(() => cli.command('bt'))
    .then(() => {
      t.match(cli.output, `#0 topFn ${script}:7:2`);
    })
    .then(() => cli.command('backtrace'))
    .then(() => {
      t.match(cli.output, `#0 topFn ${script}:7:2`);
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
