'use strict';

// Refs: https://github.com/nodejs/node/issues/39272

const common = require('../common');

const assert = require('assert');

// When this is moved out of known_issues, this skip can be removed.
if (common.isOSX) {
  assert.fail('does not fail reliably on macOS in CI');
}

// When this is moved out of known_issues, this can be removed and replaced with
// the commented-out use of common.skipIfInspectorDisabled() below.
if (!process.features.inspector) {
  assert.fail('Known issues test should fail, so if the inspector is disabled');
}

// Will need to uncomment this when moved out of known_issues.
// common.skipIfInspectorDisabled();

// This can be reduced to 2 or even 1 (and the loop removed) once the debugger
// is fixed. It's set higher to make sure that the error is tripped reliably
// in CI. On most systems, the error will be tripped on the first test, but
// on a few platforms in CI, it needs to be many times.
const RESTARTS = 16;

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
