'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

// Random port with --inspect-port=0.
{
  const script = fixtures.path('debugger', 'three-lines.js');

  const cli = startCLI(['--inspect-port=0', script]);

  cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => {
      assert.match(cli.output, /debug>/, 'prints a prompt');
      assert.match(
        cli.output,
        /< Debugger listening on /,
        'forwards child output');
    })
    .then(() => cli.quit())
    .then((code) => {
      assert.strictEqual(code, 0);
    });
}
