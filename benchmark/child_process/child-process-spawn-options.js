'use strict';
const common = require('../common.js');
const { spawn } = require('child_process');

// Isolates the cost of marshaling spawn() options across the JS -> C++ boundary
// (ProcessWrap::Spawn). A trivial, fast-exiting child is spawned repeatedly
// while scaling the number of environment pairs and arguments that have to be
// converted, so the per-spawn option-handling overhead is the dominant cost.

const isWindows = process.platform === 'win32';
const command = isWindows ? 'cmd' : 'true';
const baseArgs = isWindows ? ['/d', '/s', '/c', 'exit'] : [];

const bench = common.createBenchmark(main, {
  n: [1000],
  envc: [0, 64, 256, 1024],
  argc: [0, 8, 64],
});

function main({ n, envc, argc }) {
  const env = { ...process.env };
  for (let i = 0; i < envc; i++)
    env[`NODE_BENCH_VAR_${i}`] = `value_${i}`;

  const args = baseArgs.slice();
  for (let i = 0; i < argc; i++)
    args.push(`arg_${i}`);

  const options = { env, stdio: ['ignore', 'ignore', 'ignore'] };

  let left = n;
  const go = () => {
    if (--left < 0)
      return bench.end(n);
    const child = spawn(command, args, options);
    // The exit code is intentionally ignored: the child only exercises the
    // option-marshaling path, it is not expected to do any useful work.
    child.on('exit', go);
  };

  bench.start();
  go();
}
