'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

const cli = startCLI(['--port=0', fixtures.path('debugger/three-lines.js')]);

(async () => {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('list(0)');
  assert.match(cli.output, /> 1 let x = 1;/);
  await cli.command('list(1)');
  assert.match(cli.output, /> 1 let x = 1;\r?\n {2}2 x = x \+ 1;/);
  await cli.command('list(10)');
  assert.match(cli.output, /> 1 let x = 1;\r?\n {2}2 x = x \+ 1;\r?\n {2}3 module\.exports = x;\r?\n {2}4 /);
  await cli.command('c');
  await cli.waitFor(/disconnect/);
  await cli.waitFor(/debug> $/);
  await cli.command('list()');
  await cli.waitFor(/ERR_DEBUGGER_ERROR/);
  assert.match(cli.output, /Uncaught Error \[ERR_DEBUGGER_ERROR\]: Requires execution to be paused/);
})()
.finally(() => cli.quit())
.then(common.mustCall());
