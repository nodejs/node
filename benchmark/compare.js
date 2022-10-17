'use strict';

const CLI = require('./_cli.js');
const { compareBenchmarks } = require('node:benchmark');
//
// Parse arguments
//
const cli = new CLI(`usage: ./node compare.js [options] [--] <category> ...
  Run each benchmark in the <category> directory many times using two different
  node versions. More than one <category> directory can be specified.
  The output is formatted as csv, which can be processed using for
  example 'compare.R'.

  --new      ./new-node-binary  new node binary (required)
  --old      ./old-node-binary  old node binary (required)
  --runs     30                 number of samples
  --filter   pattern            includes only benchmark scripts matching
                                <pattern> (can be repeated)
  --exclude  pattern            excludes scripts matching <pattern> (can be
                                repeated)
  --set      variable=value     set benchmark variable (can be repeated)
  --no-progress                 don't show benchmark progress indicator
`, { arrayArgs: ['set', 'filter', 'exclude'], boolArgs: ['no-progress'] });

if (!cli.optional.new || !cli.optional.old) {
  cli.abort(cli.usage);
}

const runs = cli.optional.runs ? parseInt(cli.optional.runs, 10) : 30;
const benchmarkFiles = cli.benchmarks();

if (benchmarkFiles.length === 0) {
  console.error('No benchmarks found');
  process.exitCode = 1;
  return;
}

compareBenchmarks(
  {
    binary: {
      old: cli.optional.old,
      new: cli.optional.new
    },
    benchmarkFiles,
    showProgress: !cli.optional['no-progress'],
    set: cli.optional.set,
    runs,
  }
);
