'use strict';
const Path = require('path');

const { test } = require('tap');

const startCLI = require('./start-cli');

test('break on (uncaught) exceptions', (t) => {
  const script = Path.join('examples', 'exceptions.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => {
      t.match(cli.output, `break in ${script}:1`);
    })
    // making sure it will die by default:
    .then(() => cli.command('c'))
    // TODO: Remove FATAL ERROR once node doesn't show a FATAL ERROR anymore
    .then(() => cli.waitFor(/disconnect|FATAL ERROR/))

    // Next run: With `breakOnException` it pauses in both places
    .then(() => cli.stepCommand('r'))
    .then(() => cli.waitForInitialBreak())
    .then(() => {
      t.match(cli.output, `break in ${script}:1`);
    })
    .then(() => cli.command('breakOnException'))
    .then(() => cli.stepCommand('c'))
    .then(() => {
      t.match(cli.output, `exception in ${script}:3`);
    })
    .then(() => cli.stepCommand('c'))
    .then(() => {
      t.match(cli.output, `exception in ${script}:9`);
    })

    // Next run: With `breakOnUncaught` it only pauses on the 2nd exception
    .then(() => cli.command('breakOnUncaught'))
    .then(() => cli.stepCommand('r')) // also, the setting survives the restart
    .then(() => cli.waitForInitialBreak())
    .then(() => {
      t.match(cli.output, `break in ${script}:1`);
    })
    .then(() => cli.stepCommand('c'))
    .then(() => {
      t.match(cli.output, `exception in ${script}:9`);
    })

    // Next run: Back to the initial state! It should die again.
    .then(() => cli.command('breakOnNone'))
    .then(() => cli.stepCommand('r'))
    .then(() => cli.waitForInitialBreak())
    .then(() => {
      t.match(cli.output, `break in ${script}:1`);
    })
    .then(() => cli.command('c'))
    // TODO: Remove FATAL ERROR once node doesn't show a FATAL ERROR anymore
    .then(() => cli.waitFor(/disconnect|FATAL ERROR/))

    .then(() => cli.quit())
    .then(null, onFatal);
});
