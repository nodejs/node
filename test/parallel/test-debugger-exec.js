'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

const cli = startCLI([fixtures.path('debugger/alive.js')]);

async function waitInitialBreak() {
  try {
    await cli.waitForInitialBreak();
    await cli.waitForPrompt();
    await cli.command('exec [typeof heartbeat, typeof process.exit]');
    assert.match(cli.output, /\[ 'function', 'function' \]/, 'works w/o paren');

    await cli.command('p [typeof heartbeat, typeof process.exit]');
    assert.match(
      cli.output,
      /\[ 'function', 'function' \]/,
      'works w/o paren, short'
    );

    await cli.command('repl');
    assert.match(
      cli.output,
      /Press Ctrl\+C to leave debug repl\n+> /,
      'shows hint for how to leave repl'
    );
    assert.doesNotMatch(cli.output, /debug>/, 'changes the repl style');

    await cli.command('[typeof heartbeat, typeof process.exit]');
    await cli.waitFor(/function/);
    await cli.waitForPrompt();
    assert.match(
      cli.output,
      /\[ 'function', 'function' \]/,
      'can evaluate in the repl'
    );
    assert.match(cli.output, /> $/);

    await cli.ctrlC();
    await cli.waitFor(/debug> $/);
    await cli.command('exec("[typeof heartbeat, typeof process.exit]")');
    assert.match(cli.output, /\[ 'function', 'function' \]/, 'works w/ paren');
    await cli.command('p("[typeof heartbeat, typeof process.exit]")');
    assert.match(
      cli.output,
      /\[ 'function', 'function' \]/,
      'works w/ paren, short'
    );

    await cli.command('cont');
    await cli.command('exec [typeof heartbeat, typeof process.exit]');
    assert.match(
      cli.output,
      /\[ 'undefined', 'function' \]/,
      'non-paused exec can see global but not module-scope values'
    );

    // Ref: https://github.com/nodejs/node/issues/46808
    await cli.waitForPrompt();
    await cli.command('exec { a: 1 }');
    assert.match(cli.output, /\{ a: 1 \}/);
  } finally {
    await cli.quit();
  }
}

waitInitialBreak();
