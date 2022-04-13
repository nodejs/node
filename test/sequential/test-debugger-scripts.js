'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

// List scripts.
{
  const script = fixtures.path('debugger', 'three-lines.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('scripts'))
    .then(() => {
      assert.match(
        cli.output,
        /^\* \d+: \S+debugger(?:\/|\\)three-lines\.js/m,
        'lists the user script');
      assert.doesNotMatch(
        cli.output,
        /\d+: node:internal\/buffer/,
        'omits node-internal scripts');
    })
    .then(() => cli.command('scripts(true)'))
    .then(() => {
      assert.match(
        cli.output,
        /\* \d+: \S+debugger(?:\/|\\)three-lines\.js/,
        'lists the user script');
      assert.match(
        cli.output,
        /\d+: node:internal\/buffer/,
        'includes node-internal scripts');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
