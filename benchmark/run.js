'use strict';

const path = require('path');
const fork = require('child_process').fork;
const CLI = require('./_cli.js');

const cli = new CLI(`usage: ./node run.js [options] [--] <category> ...
  Run each benchmark in the <category> directory a single time, more than one
  <category> directory can be specified.

  --filter   pattern        includes only benchmark scripts matching <pattern>
                            (can be repeated)
  --exclude  pattern        excludes scripts matching <pattern> (can be
                            repeated)
  --set    variable=value   set benchmark variable (can be repeated)
  --format [simple|csv]     optional value that specifies the output format
  test                      only run a single configuration from the options
                            matrix
  all                       each benchmark category is run one after the other
`, { arrayArgs: ['set', 'filter', 'exclude'] });
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
  const child = fork(
    path.resolve(__dirname, filename),
    cli.test ? ['--test'] : cli.optional.set,
  );

  if (format !== 'csv') {
    console.log();
    console.log(filename);
  }

  child.on('message', (data) => {
    if (data.type !== 'report') {
      return;
    }
    // Construct configuration string, " A=a, B=b, ..."
    let conf = '';
    for (const key of Object.keys(data.conf)) {
      if (conf !== '')
        conf += ' ';
      conf += `${key}=${JSON.stringify(data.conf[key])}`;
    }
    if (format === 'csv') {
      // Escape quotes (") for correct csv formatting
      conf = conf.replace(/"/g, '""');
      console.log(`"${data.name}", "${conf}", ${data.rate}, ${data.time}`);
    } else {
      let rate = data.rate.toString().split('.');
      rate[0] = rate[0].replace(/(\d)(?=(?:\d\d\d)+(?!\d))/g, '$1,');
      rate = (rate[1] ? rate.join('.') : rate[0]);
      console.log(`${data.name} ${conf}: ${rate}`);
    }
  });

  child.once('close', (code) => {
    if (code) {
      process.exit(code);
    }

    // If there are more benchmarks execute the next
    if (i + 1 < benchmarks.length) {
      recursive(i + 1);
    }
  });
})(0);
