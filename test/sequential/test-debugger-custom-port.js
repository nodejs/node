'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

// Custom port.
const script = fixtures.path('debugger', 'three-lines.js');

const cli = startCLI([`--port=${common.PORT}`, script]);
(async function() {
  try {
    await cli.waitForInitialBreak();
    await cli.waitForPrompt();
    assert.match(cli.output, /debug>/, 'prints a prompt');
    assert.match(
      cli.output,
      new RegExp(`< Debugger listening on [^\n]*${common.PORT}`),
      'forwards child output');
  } finally {
    const code = await cli.quit();
    assert.strictEqual(code, 0);
  }
})().then(common.mustCall());
