'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

// Run after quit.
{
  const scriptFullPath = fixtures.path('debugger', 'three-lines.js');
  const script = path.relative(process.cwd(), scriptFullPath);
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('breakpoints'))
    .then(() => {
      assert.match(cli.output, /No breakpoints yet/);
    })
    .then(() => cli.command('sb(2)'))
    .then(() => cli.command('sb(3)'))
    .then(() => cli.command('breakpoints'))
    .then(() => {
      assert.ok(cli.output.includes(`#0 ${script}:2`));
      assert.ok(cli.output.includes(`#1 ${script}:3`));
    })
    .then(() => cli.stepCommand('c')) // hit line 2
    .then(() => cli.stepCommand('c')) // hit line 3
    .then(() => {
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 3 });
    })
    .then(() => cli.command('restart'))
    .then(() => cli.waitForInitialBreak())
    .then(() => {
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });
    })
    .then(() => cli.stepCommand('c'))
    .then(() => {
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 2 });
    })
    .then(() => cli.stepCommand('c'))
    .then(() => {
      assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 3 });
    })
    .then(() => cli.command('breakpoints'))
    .then(() => {
      const msg = `SCRIPT: ${script}, OUTPUT: ${cli.output}`;
      assert.ok(cli.output.includes(`#0 ${script}:2`), msg);
      assert.ok(cli.output.includes(`#1 ${script}:3`), msg);
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
