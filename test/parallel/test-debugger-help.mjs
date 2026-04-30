import { skipIfInspectorDisabled } from '../common/index.mjs';

skipIfInspectorDisabled();

import * as fixtures from '../common/fixtures.mjs';
import startCLI from '../common/debugger.js';

import assert from 'assert';

const cli = startCLI([fixtures.path('debugger', 'empty.js')]);

try {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('help');
  assert.match(cli.output, /run, restart, r\s+/m);
} finally {
  cli.quit();
}
