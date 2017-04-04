'use strict';
const { test } = require('tap');

const startCLI = require('./start-cli');

test('examples/alive.js', (t) => {
  const cli = startCLI(['examples/alive.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('exec [typeof heartbeat, typeof process.exit]'))
    .then(() => {
      t.match(cli.output, '[ \'function\', \'function\' ]', 'works w/o paren');
    })
    .then(() => cli.command('repl'))
    .then(() => {
      t.match(
        cli.output,
        'Press Ctrl + C to leave debug repl\n> ',
        'shows hint for how to leave repl');
      t.notMatch(cli.output, 'debug>', 'changes the repl style');
    })
    .then(() => cli.command('[typeof heartbeat, typeof process.exit]'))
    .then(() => cli.waitFor(/function/))
    .then(() => cli.waitForPrompt())
    .then(() => {
      t.match(
        cli.output,
        '[ \'function\', \'function\' ]', 'can evaluate in the repl');
      t.match(cli.output, /> $/);
    })
    .then(() => cli.ctrlC())
    .then(() => cli.waitFor(/debug> $/))
    .then(() => cli.command('exec("[typeof heartbeat, typeof process.exit]")'))
    .then(() => {
      t.match(cli.output, '[ \'function\', \'function\' ]', 'works w/ paren');
    })
    .then(() => cli.command('cont'))
    .then(() => cli.command('exec [typeof heartbeat, typeof process.exit]'))
    .then(() => {
      t.match(
        cli.output,
        '[ \'undefined\', \'function\' ]',
        'non-paused exec can see global but not module-scope values');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});

test('exec .scope', (t) => {
  const cli = startCLI(['examples/backtrace.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.stepCommand('c'))
    .then(() => cli.command('exec .scope'))
    .then(() => {
      t.match(
        cli.output,
        '\'moduleScoped\'', 'displays closure from module body');
      t.match(cli.output, '\'a\'', 'displays local / function arg');
      t.match(cli.output, '\'l1\'', 'displays local scope');
      t.notMatch(cli.output, '\'encodeURIComponent\'', 'omits global scope');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
