'use strict';
const { spawn } = require('child_process');
const Path = require('path');

const { test } = require('tap');

const startCLI = require('./start-cli');

function launchTarget(...args) {
  const childProc = spawn(process.execPath, args);
  return Promise.resolve(childProc);
}

// process.debugPort is our proxy for "the version of node used to run this
// test suite doesn't support SIGUSR1 for enabling --inspect for a process".
const defaultsToOldProtocol = process.debugPort === 5858;

test('examples/alive.js', { skip: defaultsToOldProtocol }, (t) => {
  const script = Path.join('examples', 'alive.js');
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
    if (error) throw error;
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
      t.match(
        cli.output,
        '> 3   ++x;',
        'marks the 3rd line');
    })
    .then(() => cleanup())
    .then(null, cleanup);
});
