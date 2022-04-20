'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

const cli = startCLI([fixtures.path('debugger/three-lines.js')]);

cli.waitForInitialBreak()
  .then(() => cli.waitForPrompt())
  .then(() => cli.command('list(0)'))
  .then(() => {
    assert.match(cli.output, /> 1 let x = 1;/);
  })
  .then(() => cli.command('list(1)'))
  .then(() => {
    assert.match(cli.output, /> 1 let x = 1;\n {2}2 x = x \+ 1;/);
  })
  .then(() => cli.command('list(10)'))
  .then(() => {
    assert.match(cli.output, /> 1 let x = 1;\n {2}2 x = x \+ 1;\n {2}3 module\.exports = x;\n {2}4 /);
  })
  .then(() => cli.command('c'))
  .then(() => cli.waitFor(/disconnect/))
  .then(() => cli.waitFor(/debug> $/))
  .then(() => cli.command('list()'))
  .then(() => {
    assert.match(cli.output, /Uncaught Error \[ERR_DEBUGGER_ERROR\]: Requires execution to be paused/);
  })
  .finally(() => cli.quit())
  .then(common.mustCall());
