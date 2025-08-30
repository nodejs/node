'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [1000],
});

const spawn = require('child_process').spawn;
const isWindows = process.platform === 'win32';

function main({ n }) {
  bench.start();
  go(n, n);
}

function go(n, left) {
  if (--left === 0)
    return bench.end(n);

  const child = isWindows ?
    spawn('cmd.exe', ['/c', 'echo', 'hello']) :
    spawn('echo', ['hello']);
  child.on('exit', (code) => {
    if (code)
      process.exit(code);
    else
      go(n, left);
  });
}
