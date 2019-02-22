'use strict';
const common = require('../common.js');
const { spawn } = require('child_process');
const path = require('path');

let Worker;  // Lazy loaded in main

const bench = common.createBenchmark(main, {
  dur: [1],
  script: ['benchmark/fixtures/require-cachable', 'test/fixtures/semicolon'],
  mode: ['process', 'worker']
}, {
  flags: ['--expose-internals']
});

function spawnProcess(script) {
  const cmd = process.execPath || process.argv[0];
  const argv = ['--expose-internals', script];
  return spawn(cmd, argv);
}

function spawnWorker(script) {
  return new Worker(script, { stderr: true, stdout: true });
}

function start(state, script, bench, getNode) {
  const node = getNode(script);
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
      start(state, script, bench, getNode);
    } else {
      bench.end(state.throughput);
    }
  });
}

function main({ dur, script, mode }) {
  const state = {
    go: true,
    throughput: 0
  };

  setTimeout(() => {
    state.go = false;
  }, dur * 1000);

  script = path.resolve(__dirname, '../../', `${script}.js`);
  if (mode === 'worker') {
    Worker = require('worker_threads').Worker;
    bench.start();
    start(state, script, bench, spawnWorker);
  } else {
    bench.start();
    start(state, script, bench, spawnProcess);
  }
}
