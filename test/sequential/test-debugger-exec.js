'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

{

  const cli = startCLI([fixtures.path('debugger/alive.js')]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('exec [typeof heartbeat, typeof process.exit]'))
    .then(() => {
      assert.match(
        cli.output,
        /\[ 'function', 'function' \]/,
        'works w/o paren'
      );
    })
    .then(() => cli.command('p [typeof heartbeat, typeof process.exit]'))
    .then(() => {
      assert.match(
        cli.output,
        /\[ 'function', 'function' \]/,
        'works w/o paren, short'
      );
    })
    .then(() => cli.command('repl'))
    .then(() => {
      assert.match(
        cli.output,
        /Press Ctrl\+C to leave debug repl\n+> /,
        'shows hint for how to leave repl');
      assert.doesNotMatch(cli.output, /debug>/, 'changes the repl style');
    })
    .then(() => cli.command('[typeof heartbeat, typeof process.exit]'))
    .then(() => cli.waitFor(/function/))
    .then(() => cli.waitForPrompt())
    .then(() => {
      assert.match(
        cli.output,
        /\[ 'function', 'function' \]/, 'can evaluate in the repl');
      assert.match(cli.output, /> $/);
    })
    .then(() => cli.ctrlC())
    .then(() => cli.waitFor(/debug> $/))
    .then(() => cli.command('exec("[typeof heartbeat, typeof process.exit]")'))
    .then(() => {
      assert.match(
        cli.output,
        /\[ 'function', 'function' \]/,
        'works w/ paren'
      );
    })
    .then(() => cli.command('p("[typeof heartbeat, typeof process.exit]")'))
    .then(() => {
      assert.match(
        cli.output,
        /\[ 'function', 'function' \]/,
        'works w/ paren, short'
      );
    })
    .then(() => cli.command('cont'))
    .then(() => cli.command('exec [typeof heartbeat, typeof process.exit]'))
    .then(() => {
      assert.match(
        cli.output,
        /\[ 'undefined', 'function' \]/,
        'non-paused exec can see global but not module-scope values');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
