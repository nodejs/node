'use strict';
const { spawn } = require('child_process');
const Path = require('path');
const { test } = require('tap');

const startCLI = require('./start-cli');

// NOTE(oyyd): We might want to import this regexp from "lib/_inspect.js"?
const kDebuggerMsgReg = /Debugger listening on ws:\/\/\[?(.+?)\]?:(\d+)\//;

function launchTarget(...args) {
  const childProc = spawn(process.execPath, args);
  return new Promise((resolve, reject) => {
    const onExit = () => {
      reject(new Error('Child process exits unexpectly'));
    };
    childProc.on('exit', onExit);
    childProc.stderr.setEncoding('utf8');
    childProc.stderr.on('data', (data) => {
      const ret = kDebuggerMsgReg.exec(data);
      childProc.removeListener('exit', onExit);
      if (ret) {
        resolve({
          childProc,
          host: ret[1],
          port: ret[2],
        });
      }
    });
  });
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

  return launchTarget('--inspect=0', script)
    .then(({ childProc, host, port }) => {
      target = childProc;
      cli = startCLI([`${host || '127.0.0.1'}:${port}`]);
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
