// Tests that --help / -h are forwarded to the debuggee when a script is
// also passed, instead of being hijacked as the inspect help flag.

import { skipIfInspectorDisabled } from '../common/index.mjs';

skipIfInspectorDisabled();

import * as fixtures from '../common/fixtures.mjs';
import startCLI from '../common/debugger.js';

import assert from 'assert';

async function getEvaluatedArgv(flag) {
  const script = fixtures.path('debugger', 'empty.js');
  const cli = startCLI([script, flag]);
  try {
    await cli.waitForInitialBreak();
    await cli.waitForPrompt();
    await cli.command('exec process.argv');
    return cli.output;
  } finally {
    await cli.quit();
  }
}

async function checkForwardedHelp(flag) {
  const output = await getEvaluatedArgv(flag);
  assert(output.includes(`'${flag}'`),
         `expected debuggee process.argv to include "${flag}", got:\n${output}`);
  assert.doesNotMatch(output, /Usage: .+ inspect/);
}

await checkForwardedHelp('--help');
await checkForwardedHelp('-h');
