'use strict';

const fork = require('child_process').fork;
const path = require('path');
const CLI = require('./_cli.js');

//
// Parse arguments
//
const cli = new CLI(`usage: ./node scatter.js [options] [--] <filename>
  Run the benchmark script <filename> many times and output the rate (ops/s)
  together with the benchmark variables as a csv.

  --runs 30              number of samples
  --set  variable=value  set benchmark variable (can be repeated)
`, { arrayArgs: ['set'] });

if (cli.items.length !== 1) {
  cli.abort(cli.usage);
}

// Create queue from the benchmarks list such both node versions are tested
// `runs` amount of times each.
const filepath = path.resolve(cli.items[0]);
const name = filepath.slice(__dirname.length + 1);
const runs = cli.optional.runs ? parseInt(cli.optional.runs, 10) : 30;

let printHeader = true;

function csvEncodeValue(value) {
  if (typeof value === 'number') {
    return value.toString();
  }
  return `"${value.replace(/"/g, '""')}"`;
}

(function recursive(i) {
  const child = fork(path.resolve(__dirname, filepath), cli.optional.set);

  child.on('message', (data) => {
    if (data.type !== 'report') {
      return;
    }

    // print csv header
    if (printHeader) {
      const confHeader = Object.keys(data.conf)
        .map(csvEncodeValue)
        .join(', ');
      console.log(`"filename", ${confHeader}, "rate", "time"`);
      printHeader = false;
    }

    // print data row
    const confData = Object.keys(data.conf)
      .map((key) => csvEncodeValue(data.conf[key]))
      .join(', ');

    console.log(`"${name}", ${confData}, ${data.rate}, ${data.time}`);
  });

  child.once('close', (code) => {
    if (code) {
      process.exit(code);
      return;
    }

    // If there are more benchmarks execute the next
    if (i + 1 < runs) {
      recursive(i + 1);
    }
  });
})(0);
