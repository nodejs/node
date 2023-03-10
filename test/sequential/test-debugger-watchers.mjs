import { skipIfInspectorDisabled } from '../common/index.mjs';
skipIfInspectorDisabled();

import { path } from '../common/fixtures.mjs';
import startCLI from '../common/debugger.js';

import assert from 'assert';

const script = path('debugger', 'break.js');
const cli = startCLI([script]);

function onFatal(error) {
  cli.quit();
  throw error;
}

// Stepping through breakpoints.
try {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('watch("x")');
  await cli.command('watch("\\"Hello\\"")');
  await cli.command('watch("42")');
  await cli.command('watch("NaN")');
  await cli.command('watch("true")');
  await cli.command('watch("[1, 2]")');
  await cli.command('watch("process.env")');
  await cli.command('watchers');

  assert.match(cli.output, /x is not defined/);
  assert.match(cli.output, /1: "Hello" = 'Hello'/);
  assert.match(cli.output, /2: 42 = 42/);
  assert.match(cli.output, /3: NaN = NaN/);
  assert.match(cli.output, /4: true = true/);
  assert.match(cli.output, /5: \[1, 2\] = \[ 1, 2 \]/);
  assert.match(cli.output, /6: process\.env =\n\s+\{[\s\S]+,\n\s+\.\.\. \}/,
               'shows "..." for process.env');

  await cli.command('unwatch(4)');
  await cli.command('unwatch("42")');
  await cli.stepCommand('n');

  assert.match(cli.output, /0: x = 10/);
  assert.match(cli.output, /1: "Hello" = 'Hello'/);
  assert.match(cli.output, /2: NaN = NaN/);
  assert.match(cli.output, /3: \[1, 2\] = \[ 1, 2 \]/);
  assert.match(
    cli.output,
    /4: process\.env =\n\s+\{[\s\S]+,\n\s+\.\.\. \}/,
    'shows "..." for process.env'
  );

  await cli.quit();
} catch (error) {
  onFatal(error);
}
