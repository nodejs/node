'use strict';

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');

function runBenchmark(name, env) {
  const argv = ['test'];

  argv.push(name);

  const mergedEnv = { ...process.env, ...env };

  const child = fork(runjs, argv, {
    env: mergedEnv,
    stdio: ['inherit', 'pipe', 'inherit', 'ipc'],
  });
  child.stdout.setEncoding('utf8');

  let stdout = '';
  child.stdout.on('data', (line) => {
    stdout += line;
  });

  child.on('exit', (code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    // This bit makes sure that each benchmark file is being sent settings such
    // that the benchmark file runs just one set of options. This helps keep the
    // benchmark tests from taking a long time to run. Therefore, each benchmark
    // file should result in three lines of output: a blank line, a line with
    // the name of the benchmark file, and from 1 to N lines with the only results that we
    // get from testing the benchmark file, depending on how many groups the benchmark has.
    assert.ok(
      /^(?:\n.+?\n.+?\n)+\n.+?$/gm.test(stdout),
      `benchmark file not running exactly one configuration in test: ${stdout}`,
    );
  });
}

module.exports = runBenchmark;
