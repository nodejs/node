'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

{
  const script = fixtures.path('debugger', 'three-lines.js');
  const cli = startCLI([script]);

  cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => {
      assert.match(cli.output, /debug>/, 'prints a prompt');
      assert.match(
        cli.output,
        /< Debugger listening on [^\n]*9229/,
        'forwards child output');
    })
    .then(() => cli.command('["hello", "world"].join(" ")'))
    .then(() => {
      assert.match(cli.output, /hello world/, 'prints the result');
    })
    .then(() => cli.command(''))
    .then(() => {
      assert.match(
        cli.output,
        /hello world/,
        'repeats the last command on <enter>'
      );
    })
    .then(() => cli.command('version'))
    .then(() => {
      assert.ok(
        cli.output.includes(process.versions.v8),
        'version prints the v8 version'
      );
    })
    .then(() => cli.quit())
    .then((code) => {
      assert.strictEqual(code, 0);
    });
}
