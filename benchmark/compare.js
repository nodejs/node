'use strict';

const fork = require('child_process').fork;
const path = require('path');
const CLI = require('./_cli.js');
const BenchmarkProgress = require('./_benchmark_progress.js');
const TTest = require('./t-test');

//
// Parse arguments
//
const cli = CLI(`usage: ./node compare.js [options] [--] <category> ...
  Run each benchmark in the <category> directory many times using two different
  node versions. More than one <category> directory can be specified.
  The output is formatted as csv, which can be processed using for
  example 'compare.R'.

  --new      ./new-node-binary  new node binary (required)
  --old      ./old-node-binary  old node binary (required)
  --runs     30                 number of samples
  --filter   pattern            string to filter benchmark scripts
  --set      variable=value     set benchmark variable (can be repeated)
  --no-progress                 don't show benchmark progress indicator
  --stats                       calculate the improvement and confidence
`, {
  arrayArgs: ['set'],
  boolArgs: ['no-progress', 'stats']
});

if (!cli.optional.new || !cli.optional.old) {
  cli.abort(cli.usage);
  return;
}

const binaries = ['old', 'new'];
const runs = cli.optional.runs ? parseInt(cli.optional.runs, 10) : 30;
const benchmarks = cli.benchmarks();

if (benchmarks.length === 0) {
  console.error('No benchmarks found');
  process.exitCode = 1;
  return;
}

// Create queue from the benchmarks list such both node versions are tested
// `runs` amount of times each.
// Note: BenchmarkProgress relies on this order to estimate
// how much runs remaining for a file. All benchmarks generated from
// the same file must be run consecutively.
const queue = [];
for (const filename of benchmarks) {
  for (let iter = 0; iter < runs; iter++) {
    for (const binary of binaries) {
      queue.push({ binary, filename, iter });
    }
  }
}
// queue.length = binary.length * runs * benchmarks.length

const kStartOfQueue = 0;

const showProgress = !cli.optional['no-progress'];
const showStats = cli.optional['stats'];

// Print csv header
if (!showStats)
  console.log('"binary", "filename", "configuration", "rate", "time"');

let progress;
if (showProgress) {
  progress = new BenchmarkProgress(queue, benchmarks, showStats);
  progress.startQueue(kStartOfQueue);
}

const outputs = [];

function calculateStats() {
  const stats = {};
  var maxLength = 0;
  var i;
  for (i = 0; i < outputs.length; i++) {
    var job = outputs[i];
    if (!stats[job.conf]) {
      stats[job.conf] = {
        conf: job.conf,
        filename: job.filename,
        improvement: 0,
        pValue: 0,
        confidence: '   ',
        old: {
          count: 0,
          total: 0,
          mean: 0,
          values: []
        },
        new: {
          count: 0,
          total: 0,
          mean: 0,
          values: []
        }
      };
    }
    if (job.conf.length + job.filename.length > maxLength)
      maxLength = job.conf.length + job.filename.length;
    const stat = stats[job.conf][job.binary];
    stat.count++;
    stat.total += job.rate;
    stat.values.push(job.rate);
  }

  var consoleOutputs = [];
  var maxLengthImprovement = 0;
  for (var name in stats) {
    const stat = stats[name];
    stat.old.mean = stat.old.total / stat.old.count;
    stat.new.mean = stat.new.total / stat.new.count;
    stat.improvement = (stat.new.mean - stat.old.mean) / stat.old.mean * 100;
    stat.improvementStr = '' + stat.improvement.toFixed(2) + ' %';
    if (stat.improvementStr.length > maxLengthImprovement)
      maxLengthImprovement = stat.improvementStr.length;
    var ttest = new TTest(stat.old.values, stat.new.values);
    stat.pValue = ttest.pValue();
    stat.pValueStr = stat.pValue.toExponential();
    if (stat.pValue < 0.001) {
      stat.confidence = '***';
    } else if (stat.pValue < 0.01) {
      stat.confidence = ' **';
    } else if (stat.pValue < 0.05) {
      stat.confidence = '  *';
    }
    consoleOutputs.push({
      name: ' ' + stat.filename + ' ' + stat.conf,
      improvement: stat.improvementStr,
      confidence: stat.confidence,
      pValue: stat.pValueStr
    });
  }
  maxLength += 7;
  var current;
  var s = '';
  while (s.length < maxLength - 5)
    s += ' ';
  s += 'improvement confidence      p.value';
  console.log(s);
  for (i = consoleOutputs.length - 1; i >= 0; i--) {
    current = consoleOutputs[i];
    s = current.name;
    while (s.length < maxLength)
      s += ' ';
    while (current.improvement.length < maxLengthImprovement)
      current.improvement = ' ' + current.improvement;
    s += current.improvement + '       ' + current.confidence + ' ' +
         current.pValue;
    console.log(s);
  }
}

(function recursive(i) {
  const job = queue[i];

  const child = fork(path.resolve(__dirname, job.filename), cli.optional.set, {
    execPath: cli.optional[job.binary]
  });

  child.on('message', function(data) {
    if (data.type === 'report') {
      // Construct configuration string, " A=a, B=b, ..."
      let conf = '';
      for (const key of Object.keys(data.conf)) {
        conf += ` ${key}=${JSON.stringify(data.conf[key])}`;
      }
      conf = conf.slice(1);

      if (showStats) {
        outputs.push({
          binary: job.binary,
          filename: job.filename,
          conf: conf,
          rate: data.rate,
          time: data.time
        });
      } else {
        // Escape quotes (") for correct csv formatting
        conf = conf.replace(/"/g, '""');
        console.log(`"${job.binary}", "${job.filename}", "${conf}", ${
                     data.rate}, ${data.time}`);
      }

      if (showProgress) {
        // One item in the subqueue has been completed.
        progress.completeConfig(data);
      }
    } else if (showProgress && data.type === 'config') {
      // The child has computed the configurations, ready to run subqueue.
      progress.startSubqueue(data, i);
    }
  });

  child.once('close', function(code) {
    if (code) {
      process.exit(code);
      return;
    }
    if (showProgress) {
      progress.completeRun(job);
    }

    // If there are more benchmarks execute the next
    if (i + 1 < queue.length) {
      recursive(i + 1);
    }
  });
})(kStartOfQueue);
