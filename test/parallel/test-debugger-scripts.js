'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

// List scripts.
{
  const script = fixtures.path('debugger', 'three-lines.js');
  const cli = startCLI(['--port=0', script]);

  (async () => {
    try {
      await cli.waitForInitialBreak();
      await cli.waitForPrompt();
      await cli.command('scripts');
      assert.match(
        cli.output,
        /^\* \d+: \S+debugger(?:\/|\\)three-lines\.js/m,
        'lists the user script');
      assert.doesNotMatch(
        cli.output,
        /\d+: node:internal\/buffer/,
        'omits node-internal scripts');
      await cli.command('scripts(true)');
      assert.match(
        cli.output,
        /\* \d+: \S+debugger(?:\/|\\)three-lines\.js/,
        'lists the user script');
      assert.match(
        cli.output,
        /\d+: node:internal\/buffer/,
        'includes node-internal scripts');
    } finally {
      await cli.quit();
    }
  })().then(common.mustCall());
}
