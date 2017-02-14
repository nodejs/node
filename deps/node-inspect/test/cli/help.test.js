'use strict';
const { test } = require('tap');

const startCLI = require('./start-cli');

test('examples/empty.js', (t) => {
  const cli = startCLI(['examples/empty.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('help'))
    .then(() => {
      t.match(cli.output, /run, restart, r\s+/m);
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
