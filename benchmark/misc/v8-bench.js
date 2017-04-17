// compare with "google-chrome deps/v8/benchmarks/run.html"
'use strict';
var fs = require('fs');
var path = require('path');
var vm = require('vm');
var common = require('../common.js');

var dir = path.join(__dirname, '..', '..', 'deps', 'v8', 'benchmarks');

function load(filename, inGlobal) {
  var source = fs.readFileSync(path.join(dir, filename), 'utf8');
  if (!inGlobal) source = `(function () {${source}\n})()`;
  vm.runInThisContext(source, { filename: `v8/bechmark/${filename}` });
}

load('base.js', true);
load('richards.js');
load('deltablue.js');
load('crypto.js');
load('raytrace.js');
load('earley-boyer.js');
load('regexp.js');
load('splay.js');
load('navier-stokes.js');

const benchmark_name = path.join('misc', 'v8-bench.js');
const times = {};
global.BenchmarkSuite.RunSuites({
  NotifyStart: function(name) {
    times[name] = process.hrtime();
  },
  NotifyResult: function(name, result) {
    const elapsed = process.hrtime(times[name]);
    common.sendResult({
      name: benchmark_name,
      conf: {
        benchmark: name
      },
      rate: result,
      time: elapsed[0] + elapsed[1] / 1e9
    });
  },
  NotifyError: function(name, error) {
    console.error(`${name}: ${error}`);
  },
  NotifyScore: function(score) {
    common.sendResult({
      name: benchmark_name,
      conf: {
        benchmark: `Score (version ${global.BenchmarkSuite.version})`
      },
      rate: score,
      time: 0
    });
  }
});
