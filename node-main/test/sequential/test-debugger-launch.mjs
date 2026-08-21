import { skipIfInspectorDisabled } from '../common/index.mjs';
skipIfInspectorDisabled();

// This must be in sequential because we check that the default port is 9229.
import { path } from '../common/fixtures.mjs';
import startCLI from '../common/debugger.js';

import assert from 'assert';

const script = path('debugger', 'three-lines.js');
const cli = startCLI([script], [], {}, { randomPort: false });
try {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  assert.match(cli.output, /debug>/, 'prints a prompt');
  assert.match(
    cli.output,
    /< Debugger listening on [^\n]*9229/,
    'forwards child output'
  );
  await cli.command('["hello", "world"].join(" ")');
  assert.match(cli.output, /hello world/, 'prints the result');
  await cli.command('');
  assert.match(
    cli.output,
    /hello world/,
    'repeats the last command on <enter>'
  );
  await cli.command('version');
  assert.ok(
    cli.output.includes(process.versions.v8),
    'version prints the v8 version'
  );
} finally {
  const code = await cli.quit();
  assert.strictEqual(code, 0);
}
