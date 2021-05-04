'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

// Break on (uncaught) exceptions.
{
  const scriptFullPath = fixtures.path('debugger', 'exceptions.js');
  const script = path.relative(process.cwd(), scriptFullPath);
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => {
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });
    })
    // Making sure it will die by default:
    .then(() => cli.command('c'))
    // TODO: Remove FATAL ERROR once node doesn't show a FATAL ERROR anymore.
    .then(() => cli.waitFor(/disconnect|FATAL ERROR/))

    // Next run: With `breakOnException` it pauses in both places.
    .then(() => cli.stepCommand('r'))
    .then(() => cli.waitForInitialBreak())
    .then(() => {
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });
    })
    .then(() => cli.command('breakOnException'))
    .then(() => cli.stepCommand('c'))
    .then(() => {
      assert.ok(cli.output.includes(`exception in ${script}:3`));
    })
    .then(() => cli.stepCommand('c'))
    .then(() => {
      assert.ok(cli.output.includes(`exception in ${script}:9`));
    })

    // Next run: With `breakOnUncaught` it only pauses on the 2nd exception.
    .then(() => cli.command('breakOnUncaught'))
    .then(() => cli.stepCommand('r')) // Also, the setting survives the restart.
    .then(() => cli.waitForInitialBreak())
    .then(() => {
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });
    })
    .then(() => cli.stepCommand('c'))
    .then(() => {
      assert.ok(cli.output.includes(`exception in ${script}:9`));
    })

    // Next run: Back to the initial state! It should die again.
    .then(() => cli.command('breakOnNone'))
    .then(() => cli.stepCommand('r'))
    .then(() => cli.waitForInitialBreak())
    .then(() => {
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });
    })
    .then(() => cli.command('c'))
    // TODO: Remove FATAL ERROR once node doesn't show a FATAL ERROR anymore
    .then(() => cli.waitFor(/disconnect|FATAL ERROR/))

    .then(() => cli.quit())
    .then(null, onFatal);
}
