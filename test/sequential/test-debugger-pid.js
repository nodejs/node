'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const { spawn } = require('child_process');

const script = fixtures.path('debugger', 'alive.js');

const runTest = async () => {
  const target = spawn(process.execPath, [script]);
  const cli = startCLI(['-p', `${target.pid}`], [], {}, { randomPort: false });

  try {
    await cli.waitForPrompt();
    await cli.command('sb("alive.js", 3)');
    await cli.waitFor(/break/);
    await cli.waitForPrompt();
    assert.match(
      cli.output,
      /> 3 {3}\+\+x;/,
      'marks the 3rd line');
  } catch (error) {
    assert.ifError(error);
  } finally {
    await cli.quit();
    target.kill();
  }
};

runTest().then(common.mustCall());
