import { skipIfInspectorDisabled } from '../common/index.mjs';

skipIfInspectorDisabled();

import { path } from '../common/fixtures.mjs';
import startCLI from '../common/debugger.js';

import assert from 'assert';

const cli = startCLI(['--port=0', path('debugger/backtrace.js')]);

try {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.stepCommand('c');
  await cli.command('exec .scope');
  assert.match(cli.output, /'moduleScoped'/, 'displays closure from module body');
  assert.match(cli.output, /'a'/, 'displays local / function arg');
  assert.match(cli.output, /'l1'/, 'displays local scope');
  assert.doesNotMatch(cli.output, /'encodeURIComponent'/, 'omits global scope');
} finally {
  await cli.quit();
}
