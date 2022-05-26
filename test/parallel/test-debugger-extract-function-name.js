'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

const cli = startCLI([fixtures.path('debugger', 'three-lines.js')]);

function onFatal(error) {
  cli.quit();
  throw error;
}

cli.waitForInitialBreak()
  .then(() => cli.waitForPrompt())
  .then(() => cli.command('exec a = function func() {}; a;'))
  .then(() => assert.match(cli.output, /\[Function: func\]/))
  .then(() => cli.command('exec a = function func () {}; a;'))
  .then(() => assert.match(cli.output, /\[Function: func\]/))
  .then(() => cli.command('exec a = function() {}; a;'))
  .then(() => assert.match(cli.output, /\[Function: \(anonymous\)\]/))
  .then(() => cli.command('exec a = function () {}; a;'))
  .then(() => assert.match(cli.output, /\[Function: \(anonymous\)\]/))
  .then(() => cli.command('exec a = () => {}; a;'))
  .then(() => assert.match(cli.output, /\[Function: \(anonymous\)\]/))
  .then(() => cli.command('exec () => {};'))
  .then(() => assert.match(cli.output, /\[Function: \(anonymous\)\]/))
  .then(() => cli.command('exec a = function* func() {}; a;'))
  .then(() => assert.match(cli.output, /\[GeneratorFunction: func\]/))
  .then(() => cli.command('exec a = function *func() {}; a;'))
  .then(() => assert.match(cli.output, /\[GeneratorFunction: func\]/))
  .then(() => cli.command('exec a = function*func() {}; a;'))
  .then(() => assert.match(cli.output, /\[GeneratorFunction: func\]/))
  .then(() => cli.command('exec a = function * func() {}; a;'))
  .then(() => assert.match(cli.output, /\[GeneratorFunction: func\]/))
  .then(() => cli.quit())
  .then(null, onFatal);
