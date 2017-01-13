'use strict';

const fork = require('child_process').fork;
const path = require('path');
const CLI = require('./_cli.js');

//
// Parse arguments
//
const cli = CLI(`usage: ./node compare.js [options] [--] <category> ...
  Run each benchmark in the <category> directory many times using two diffrent
  node versions. More than one <category> directory can be specified.
  The output is formatted as csv, which can be processed using for
  example 'compare.R'.

  --new    ./new-node-binary  new node binary (required)
  --old    ./old-node-binary  old node binary (required)
  --runs   30                 number of samples
  --filter pattern            string to filter benchmark scripts
  --set    variable=value     set benchmark variable (can be repeated)
`, {
  arrayArgs: ['set']
});

if (!cli.optional.new || !cli.optional.old) {
  cli.abort(cli.usage);
  return;
}

const binaries = ['old', 'new'];
const runs = cli.optional.runs ? parseInt(cli.optional.runs, 10) : 30;
const benchmarks = cli.benchmarks();

if (benchmarks.length === 0) {
  console.error('no benchmarks found');
  process.exitCode = 1;
  return;
}

// Create queue from the benchmarks list such both node versions are tested
// `runs` amount of times each.
const queue = [];
for (const filename of benchmarks) {
  for (let iter = 0; iter < runs; iter++) {
    for (const binary of binaries) {
      queue.push({ binary, filename, iter });
    }
  }
}

// Print csv header
console.log('"binary", "filename", "configuration", "rate", "time"');

(function recursive(i) {
  const job = queue[i];

  const child = fork(path.resolve(__dirname, job.filename), cli.optional.set, {
    execPath: cli.optional[job.binary]
  });

  child.on('message', function(data) {
    // Construct configuration string, " A=a, B=b, ..."
    let conf = '';
    for (const key of Object.keys(data.conf)) {
      conf += ' ' + key + '=' + JSON.stringify(data.conf[key]);
    }
    conf = conf.slice(1);

    // Escape quotes (") for correct csv formatting
    conf = conf.replace(/"/g, '""');

    console.log(`"${job.binary}", "${job.filename}", "${conf}", ` +
                `${data.rate}, ${data.time}`);
  });

  child.once('close', function(code) {
    if (code) {
      process.exit(code);
      return;
    }

    // If there are more benchmarks execute the next
    if (i + 1 < queue.length) {
      recursive(i + 1);
    }
  });
})(0);
