import { skipIfInspectorDisabled } from '../common/index.mjs';

skipIfInspectorDisabled();

import { path } from '../common/fixtures.mjs';
import startCLI from '../common/debugger.js';

import assert from 'assert';

const cli = startCLI(['--port=0', path('debugger', 'empty.js')]);

try {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('help');
  assert.match(cli.output, /run, restart, r\s+/m);
} finally {
  cli.quit();
}
