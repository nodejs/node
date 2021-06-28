'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

// exec .scope
{
  const cli = startCLI([fixtures.path('debugger/backtrace.js')]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.stepCommand('c'))
    .then(() => cli.command('exec .scope'))
    .then(() => {
      assert.match(
        cli.output,
        /'moduleScoped'/, 'displays closure from module body');
      assert.match(cli.output, /'a'/, 'displays local / function arg');
      assert.match(cli.output, /'l1'/, 'displays local scope');
      assert.doesNotMatch(
        cli.output,
        /'encodeURIComponent'/,
        'omits global scope'
      );
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
