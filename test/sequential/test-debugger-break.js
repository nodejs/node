'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

// Stepping through breakpoints.
{
  const scriptFullPath = fixtures.path('debugger', 'break.js');
  const script = path.relative(process.cwd(), scriptFullPath);
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => {
      assert.deepStrictEqual(
        cli.breakInfo,
        { filename: script, line: 1 },
      );
      assert.match(
        cli.output,
        /> 1 (?:\(function \([^)]+\) \{ )?const x = 10;/,
        'shows the source and marks the current line');
    })
    .then(() => cli.stepCommand('n'))
    .then(() => {
      assert.ok(
        cli.output.includes(`break in ${script}:2`),
        'pauses in next line of the script');
      assert.match(
        cli.output,
        /> 2 let name = 'World';/,
        'marks the 2nd line');
    })
    .then(() => cli.stepCommand('next'))
    .then(() => {
      assert.ok(
        cli.output.includes(`break in ${script}:3`),
        'pauses in next line of the script');
      assert.match(
        cli.output,
        /> 3 name = 'Robin';/,
        'marks the 3nd line');
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      assert.ok(
        cli.output.includes(`break in ${script}:10`),
        'pauses on the next breakpoint');
      assert.match(
        cli.output,
        />10 debugger;/,
        'marks the debugger line');
    })

    // Prepare additional breakpoints
    .then(() => cli.command('sb("break.js", 6)'))
    .then(() => assert.doesNotMatch(cli.output, /Could not resolve breakpoint/))
    .then(() => cli.command('sb("otherFunction()")'))
    .then(() => cli.command('sb(16)'))
    .then(() => assert.doesNotMatch(cli.output, /Could not resolve breakpoint/))
    .then(() => cli.command('breakpoints'))
    .then(() => {
      assert.ok(cli.output.includes(`#0 ${script}:6`));
      assert.ok(cli.output.includes(`#1 ${script}:16`));
    })

    .then(() => cli.command('list()'))
    .then(() => {
      assert.match(
        cli.output,
        />10 debugger;/,
        'prints and marks current line'
      );
      assert.deepStrictEqual(
        cli.parseSourceLines(),
        [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
      );
    })
    .then(() => cli.command('list(2)'))
    .then(() => {
      assert.match(
        cli.output,
        />10 debugger;/,
        'prints and marks current line'
      );
      assert.deepStrictEqual(
        cli.parseSourceLines(),
        [8, 9, 10, 11, 12],
      );
    })

    .then(() => cli.stepCommand('s'))
    .then(() => cli.stepCommand(''))
    .then(() => {
      assert.match(
        cli.output,
        /break in node:timers/,
        'entered timers.js');
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      assert.ok(
        cli.output.includes(`break in ${script}:16`),
        'found breakpoint we set above w/ line number only');
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      assert.ok(
        cli.output.includes(`break in ${script}:6`),
        'found breakpoint we set above w/ line number & script');
    })
    .then(() => cli.stepCommand(''))
    .then(() => {
      assert.ok(
        cli.output.includes(`debugCommand in ${script}:14`),
        'found function breakpoint we set above');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
