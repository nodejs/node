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
  const cli = startCLI(['--port=0', script]);

  (async () => {
    try {
      await cli.waitForInitialBreak();
      await cli.waitForPrompt();
      await cli.waitForPrompt();
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });

      // Making sure it will die by default:
      await cli.command('c');
      await cli.waitFor(/disconnect/);

      // Next run: With `breakOnException` it pauses in both places.
      await cli.stepCommand('r');
      await cli.waitForInitialBreak();
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });
      await cli.command('breakOnException');
      await cli.stepCommand('c');
      assert.ok(cli.output.includes(`exception in ${script}:3`));
      await cli.stepCommand('c');
      assert.ok(cli.output.includes(`exception in ${script}:9`));

      // Next run: With `breakOnUncaught` it only pauses on the 2nd exception.
      await cli.command('breakOnUncaught');
      await cli.stepCommand('r'); // Also, the setting survives the restart.
      await cli.waitForInitialBreak();
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });
      await cli.stepCommand('c');
      assert.ok(cli.output.includes(`exception in ${script}:9`));

      // Next run: Back to the initial state! It should die again.
      await cli.command('breakOnNone');
      await cli.stepCommand('r');
      await cli.waitForInitialBreak();
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });
      await cli.command('c');
      await cli.waitFor(/disconnect/);
    } finally {
      cli.quit();
    }
  })().then(common.mustCall());
}
