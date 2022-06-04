'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

const cli = startCLI([fixtures.path('debugger', 'three-lines.js')]);

(async () => {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('exec a = function func() {}; a;');
  assert.match(cli.output, /\[Function: func\]/);
  await cli.command('exec a = function func () {}; a;');
  assert.match(cli.output, /\[Function\]/);
  await cli.command('exec a = function() {}; a;');
  assert.match(cli.output, /\[Function: function\]/);
  await cli.command('exec a = () => {}; a;');
  assert.match(cli.output, /\[Function\]/);
  await cli.command('exec a = function* func() {}; a;');
  assert.match(cli.output, /\[GeneratorFunction: func\]/);
  await cli.command('exec a = function *func() {}; a;');
  assert.match(cli.output, /\[GeneratorFunction: \*func\]/);
  await cli.command('exec a = function*func() {}; a;');
  assert.match(cli.output, /\[GeneratorFunction: function\*func\]/);
  await cli.command('exec a = function * func() {}; a;');
  assert.match(cli.output, /\[GeneratorFunction\]/);
})()
.finally(() => cli.quit())
.then(common.mustCall());
