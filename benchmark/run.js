'use strict';

const fs = require('fs');
const path = require('path');
const child_process = require('child_process');

var outputFormat = process.env.OUTPUT_FORMAT ||
                   (+process.env.NODE_BENCH_SILENT ? 'silent' : false) ||
                   'default';

// If this is the main module, then run the benchmarks
if (module === require.main) {
  var type = process.argv[2];
  var testFilter = process.argv[3];
  if (!type) {
    console.error('usage:\n ./node benchmark/run.js <type> [testFilter]');
    process.exit(1);
  }

  var dir = path.join(__dirname, type);
  var tests = fs.readdirSync(dir);

  if (testFilter) {
    var filteredTests = tests.filter(function(item) {
      if (item.lastIndexOf(testFilter) >= 0) {
        return item;
      }
    });

    if (filteredTests.length === 0) {
      console.error('%s is not found in \n %j', testFilter, tests);
      return;
    }
    tests = filteredTests;
  }

  runBenchmarks();
}

function runBenchmarks() {
  var test = tests.shift();
  if (!test)
    return;

  if (test.match(/^[\._]/))
    return process.nextTick(runBenchmarks);

  if (outputFormat == 'default')
    console.error(type + '/' + test);

  test = path.resolve(dir, test);

  var a = (process.execArgv || []).concat(test);
  var child = child_process.spawn(process.execPath, a, { stdio: 'inherit' });
  child.on('close', function(code) {
    if (code) {
      process.exit(code);
    } else {
      console.log('');
      runBenchmarks();
    }
  });
}
