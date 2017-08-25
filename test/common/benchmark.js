/* eslint-disable required-modules */

'use strict';

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');

function runBenchmark(name, args, env) {
  const argv = [];

  for (let i = 0; i < args.length; i++) {
    argv.push('--set');
    argv.push(args[i]);
  }

  argv.push(name);

  const mergedEnv = Object.assign({}, process.env, env);

  const child = fork(runjs, argv, { env: mergedEnv });
  child.on('exit', (code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
}

module.exports = runBenchmark;
