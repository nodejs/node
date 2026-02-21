'use strict';
const common = require('../common.js');
const { spawn } = require('child_process');

const bench = common.createBenchmark(main, {
  dur: [1],
  code: ['1', '"string"', 'process.versions', 'process'],
});

function spawnProcess(code) {
  const cmd = process.execPath || process.argv[0];
  const argv = ['-p', code];
  return spawn(cmd, argv);
}

function start(state, code, bench, getNode) {
  const node = getNode(code);
  let stdout = '';
  let stderr = '';

  node.stdout.on('data', (data) => {
    stdout += data;
  });

  node.stderr.on('data', (data) => {
    stderr += data;
  });

  node.on('exit', (code) => {
    if (code !== 0) {
      console.error('------ stdout ------');
      console.error(stdout);
      console.error('------ stderr ------');
      console.error(stderr);
      throw new Error(`Error during node startup, exit code ${code}`);
    }
    state.throughput++;

    if (state.go) {
      start(state, code, bench, getNode);
    } else {
      bench.end(state.throughput);
    }
  });
}

function main({ dur, code }) {
  const state = {
    go: true,
    throughput: 0,
  };

  setTimeout(() => {
    state.go = false;
  }, dur * 1000);

  bench.start();
  start(state, code, bench, spawnProcess);
}
