'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

// Random port with --inspect-port=0.
const script = fixtures.path('debugger', 'three-lines.js');

const cli = startCLI([script]);

(async () => {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  assert.match(cli.output, /debug>/, 'prints a prompt');
  assert.match(
    cli.output,
    /< Debugger listening on /,
    'forwards child output');
  const code = await cli.quit();
  assert.strictEqual(code, 0);
})().then(common.mustCall());
