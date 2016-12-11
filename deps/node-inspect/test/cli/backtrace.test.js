'use strict';
const { test } = require('tap');

const startCLI = require('./start-cli');

test('display and navigate backtrace', (t) => {
  const cli = startCLI(['examples/backtrace.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => cli.stepCommand('c'))
    .then(() => cli.command('bt'))
    .then(() => {
      t.match(cli.output, '#0 topFn examples/backtrace.js:8:2');
    })
    .then(() => cli.command('backtrace'))
    .then(() => {
      t.match(cli.output, '#0 topFn examples/backtrace.js:8:2');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
