'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

// Profiles.
{
  const cli = startCLI([fixtures.path('debugger/empty.js')]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('exec console.profile()'))
    .then(() => {
      assert.match(cli.output, /undefined/);
    })
    .then(() => cli.command('exec console.profileEnd()'))
    .then(() => delay(250))
    .then(() => {
      assert.match(cli.output, /undefined/);
      assert.match(cli.output, /Captured new CPU profile\./);
    })
    .then(() => cli.quit())
    .then(null, onFatal);
}
