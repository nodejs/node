'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const { spawn } = require('child_process');


function launchTarget(...args) {
  const childProc = spawn(process.execPath, args);
  return Promise.resolve(childProc);
}

{
  const script = fixtures.path('debugger', 'alive.js');
  let cli = null;
  let target = null;

  function cleanup(error) {
    if (cli) {
      cli.quit();
      cli = null;
    }
    if (target) {
      target.kill();
      target = null;
    }
    assert.ifError(error);
  }

  return launchTarget(script)
    .then((childProc) => {
      target = childProc;
      cli = startCLI(['-p', `${target.pid}`]);
      return cli.waitForPrompt();
    })
    .then(() => cli.command('sb("alive.js", 3)'))
    .then(() => cli.waitFor(/break/))
    .then(() => cli.waitForPrompt())
    .then(() => {
      assert.match(
        cli.output,
        /> 3   \+\+x;/,
        'marks the 3rd line');
    })
    .then(() => cleanup())
    .then(null, cleanup);
}
