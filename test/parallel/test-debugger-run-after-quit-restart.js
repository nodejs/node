'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

// Run after quit/restart.
{
  const scriptFullPath = fixtures.path('debugger', 'three-lines.js');
  const script = path.relative(process.cwd(), scriptFullPath);
  const cli = startCLI(['--port=0', script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.stepCommand('n'))
    .then(() => {
      assert.ok(
        cli.output.includes(`break in ${script}:2`),
        'steps to the 2nd line'
      );
    })
    .then(() => cli.command('cont'))
    .then(() => cli.waitFor(/disconnect/))
    .then(() => {
      assert.match(
        cli.output,
        /Waiting for the debugger to disconnect/,
        'the child was done');
    })
    .then(() => {
      // On windows the socket won't close by itself
      return cli.command('kill');
    })
    .then(() => cli.command('cont'))
    .then(() => cli.waitFor(/start the app/))
    .then(() => {
      assert.match(cli.output, /Use `run` to start the app again/);
    })
    .then(() => cli.stepCommand('run'))
    .then(() => cli.waitForInitialBreak())
    .then(() => cli.waitForPrompt())
    .then(() => {
      assert.deepStrictEqual(
        cli.breakInfo,
        { filename: script, line: 1 },
      );
    })
    .then(() => cli.stepCommand('n'))
    .then(() => {
      assert.deepStrictEqual(
        cli.breakInfo,
        { filename: script, line: 2 },
      );
    })
    .then(() => cli.stepCommand('restart'))
    .then(() => cli.waitForInitialBreak())
    .then(() => {
      assert.deepStrictEqual(
        cli.breakInfo,
        { filename: script, line: 1 },
      );
    })
    .then(() => cli.command('kill'))
    .then(() => cli.command('cont'))
    .then(() => cli.waitFor(/start the app/))
    .then(() => {
      assert.match(cli.output, /Use `run` to start the app again/);
    })
    .then(() => cli.stepCommand('run'))
    .then(() => cli.waitForInitialBreak())
    .then(() => cli.waitForPrompt())
    .then(() => {
      assert.deepStrictEqual(
        cli.breakInfo,
        { filename: script, line: 1 },
      );
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
