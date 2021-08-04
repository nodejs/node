'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');

const RESTARTS = 10;

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

// Using `restart` should result in only one "Connect/For help" message.
{
  const script = fixtures.path('debugger', 'three-lines.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  const listeningRegExp = /Debugger listening on/g;

  cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => {
      assert.strictEqual(cli.output.match(listeningRegExp).length, 1);
    })
    .then(async () => {
      for (let i = 0; i < RESTARTS; i++) {
        await cli.stepCommand('restart');
        assert.strictEqual(cli.output.match(listeningRegExp).length, 1);
      }
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
