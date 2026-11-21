'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

const scriptFullPath = fixtures.path('debugger', 'break.js');
const script = path.relative(process.cwd(), scriptFullPath);
const cli = startCLI([script]);

(async () => {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();

  await cli.command('sb("sayHello()")');
  await cli.command('sb("otherFunction()")');
  await cli.command('breakpoints');
  assert.ok(cli.output.includes('#0 sayHello()'));
  assert.ok(cli.output.includes('#1 otherFunction()'));

  await cli.command('sb("x()")');
  assert.match(cli.output, /x is not a function/);

  await cli.command('cb("otherFunction()")');
  await cli.command('cb("missing()")');
  assert.match(cli.output, /Could not find breakpoint at missing\(\)/);
  await cli.command('breakpoints');
  assert.ok(cli.output.includes('#0 sayHello()'));
  assert.ok(!cli.output.includes('otherFunction()'));

  await cli.stepCommand('cont');
  assert.ok(
    cli.output.includes(`break in ${script}:5`),
    'pauses on the first statement of sayHello');
})().finally(() => cli.quit())
  .then(common.mustCall());
