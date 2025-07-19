'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

const scriptFullPath = fixtures.path('debugger', 'break.js');
const script = path.relative(process.cwd(), scriptFullPath);
const cli = startCLI(['--port=0', script]);

(async () => {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  assert.deepStrictEqual(
    cli.breakInfo,
    { filename: script, line: 1 },
  );
  assert.match(
    cli.output,
    /> 1 (?:\(function \([^)]+\) \{ )?const x = 10;/,
    'shows the source and marks the current line');

  await cli.stepCommand('n');
  assert.ok(
    cli.output.includes(`step in ${script}:2`),
    'pauses in next line of the script');
  assert.match(
    cli.output,
    /> 2 let name = 'World';/,
    'marks the 2nd line');

  await cli.stepCommand('next');
  assert.ok(
    cli.output.includes(`step in ${script}:3`),
    'pauses in next line of the script');
  assert.match(
    cli.output,
    /> 3 name = 'Robin';/,
    'marks the 3nd line');

  await cli.stepCommand('cont');
  assert.ok(
    cli.output.includes(`break in ${script}:10`),
    'pauses on the next breakpoint');
  assert.match(
    cli.output,
    />10 debugger;/,
    'marks the debugger line');

  await cli.command('sb("break.js", 6)');
  await cli.waitFor(/> 6.*[.\s\S]*debug>/);
  assert.doesNotMatch(cli.output, /Could not resolve breakpoint/);

  await cli.command('sb("otherFunction()")');
  await cli.command('sb(16)');
  assert.doesNotMatch(cli.output, /Could not resolve breakpoint/);

  await cli.command('breakpoints');
  assert.ok(cli.output.includes(`#0 ${script}:6`));
  assert.ok(cli.output.includes(`#1 ${script}:16`));

  await cli.command('list()');
  assert.match(
    cli.output,
    />10 debugger;/,
    'prints and marks current line'
  );
  assert.deepStrictEqual(
    cli.parseSourceLines(),
    [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
  );

  await cli.command('list(2)');
  assert.match(
    cli.output,
    />10 debugger;/,
    'prints and marks current line'
  );
  assert.deepStrictEqual(
    cli.parseSourceLines(),
    [8, 9, 10, 11, 12],
  );

  await cli.stepCommand('s');
  await cli.stepCommand('');
  assert.match(
    cli.output,
    /step in node:timers/,
    'entered timers.js');

  await cli.stepCommand('cont');
  assert.ok(
    cli.output.includes(`break in ${script}:16`),
    'found breakpoint we set above w/ line number only');

  await cli.stepCommand('cont');
  assert.ok(
    cli.output.includes(`break in ${script}:6`),
    'found breakpoint we set above w/ line number & script');

  await cli.stepCommand('');
  assert.ok(
    cli.output.includes(`debugCommand in ${script}:14`),
    'found function breakpoint we set above');
})().finally(() => cli.quit())
  .then(common.mustCall());
