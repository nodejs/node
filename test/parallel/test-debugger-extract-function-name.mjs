import { skipIfInspectorDisabled } from '../common/index.mjs';

skipIfInspectorDisabled();

import * as fixtures from '../common/fixtures.mjs';
import startCLI from '../common/debugger.js';

import assert from 'assert';

// Evaluating while the target is running can trigger a V8 scope assertion.
// Keep it paused for these formatting checks.
const cli = startCLI([fixtures.path('debugger', 'three-lines.js')]);

try {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('exec a = function func() {}; a;');
  assert.match(cli.output, /\[Function: func\]/);
  await cli.command('exec a = function func () {}; a;');
  assert.match(cli.output, /\[Function\]/);
  await cli.command('exec a = function() {}; a;');
  assert.match(cli.output, /\[Function: function\]/);
  await cli.command('exec a = () => {}; a;');
  assert.match(cli.output, /\[Function\]/);
  await cli.command('exec a = function* func() {}; a;');
  assert.match(cli.output, /\[GeneratorFunction: func\]/);
  await cli.command('exec a = function *func() {}; a;');
  assert.match(cli.output, /\[GeneratorFunction: \*func\]/);
  await cli.command('exec a = function*func() {}; a;');
  assert.match(cli.output, /\[GeneratorFunction: function\*func\]/);
  await cli.command('exec a = function * func() {}; a;');
  assert.match(cli.output, /\[GeneratorFunction\]/);
} finally {
  await cli.quit();
}
