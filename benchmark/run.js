'use strict';

const path = require('path');
const fork = require('child_process').fork;
const CLI = require('./_cli.js');

const cli = CLI(`usage: ./node run.js [options] [--] <category> ...
  Run each benchmark in the <category> directory a single time, more than one
  <categoty> directory can be specified.

  --filter pattern          string to filter benchmark scripts
  --set    variable=value   set benchmark variable (can be repeated)
  --format [simple|csv]     optional value that specifies the output format
`, {
  arrayArgs: ['set']
});
const benchmarks = cli.benchmarks();

if (benchmarks.length === 0) {
  console.error('No benchmarks found');
  process.exitCode = 1;
  return;
}

const validFormats = ['csv', 'simple'];
const format = cli.optional.format || 'simple';
if (!validFormats.includes(format)) {
  console.error('Invalid format detected');
  process.exitCode = 1;
  return;
}

if (format === 'csv') {
  console.log('"filename", "configuration", "rate", "time"');
}

(function recursive(i) {
  const filename = benchmarks[i];
  const child = fork(path.resolve(__dirname, filename), cli.optional.set);

  if (format !== 'csv') {
    console.log();
    console.log(filename);
  }

  child.on('message', function(data) {
    // Construct configuration string, " A=a, B=b, ..."
    let conf = '';
    for (const key of Object.keys(data.conf)) {
      conf += ' ' + key + '=' + JSON.stringify(data.conf[key]);
    }
    // delete first space of the configuration
    conf = conf.slice(1);
    if (format === 'csv') {
      // Escape quotes (") for correct csv formatting
      conf = conf.replace(/"/g, '""');
      console.log(`"${data.name}", "${conf}", ${data.rate}, ${data.time}`);
    } else {
      console.log(`${data.name} ${conf}: ${data.rate}`);
    }
  });

  child.once('close', function(code) {
    if (code) {
      process.exit(code);
      return;
    }

    // If there are more benchmarks execute the next
    if (i + 1 < benchmarks.length) {
      recursive(i + 1);
    }
  });
})(0);
