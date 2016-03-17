'use strict';
var assert = require('assert');
var fs = require('fs');
var path = require('path');
var child_process = require('child_process');

var outputFormat = process.env.OUTPUT_FORMAT ||
                   (+process.env.NODE_BENCH_SILENT ? 'silent' : false) ||
                   'default';

// verify outputFormat
if (['default', 'csv', 'silent'].indexOf(outputFormat) == -1) {
  throw new Error('OUTPUT_FORMAT set to invalid value');
}

exports.PORT = process.env.PORT || 12346;

// If this is the main module, then run the benchmarks
if (module === require.main) {
  var type = process.argv[2];
  var testFilter = process.argv[3];
  if (!type) {
    console.error('usage:\n ./node benchmark/common.js <type> [testFilter]');
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

function hasWrk() {
  var result = child_process.spawnSync('wrk', ['-h']);
  if (result.error && result.error.code === 'ENOENT') {
    console.error('Couldn\'t locate `wrk` which is needed for running ' +
      'benchmarks. Check benchmark/README.md for further instructions.');
    process.exit(-1);
  }
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

exports.createBenchmark = function(fn, options) {
  return new Benchmark(fn, options);
};

function Benchmark(fn, options) {
  this.fn = fn;
  this.options = options;
  this.config = parseOpts(options);
  this._name = require.main.filename.split(/benchmark[\/\\]/).pop();
  this._start = [0, 0];
  this._started = false;

  var self = this;

  process.nextTick(function() {
    self._run();
  });
}

// benchmark an http server.
Benchmark.prototype.http = function(p, args, cb) {
  hasWrk();
  var self = this;
  var regexp = /Requests\/sec:[ \t]+([0-9\.]+)/;
  var url = 'http://127.0.0.1:' + exports.PORT + p;

  args = args.concat(url);

  var out = '';
  var child = child_process.spawn('wrk', args);

  child.stdout.setEncoding('utf8');

  child.stdout.on('data', function(chunk) {
    out += chunk;
  });

  child.on('close', function(code) {
    if (cb)
      cb(code);

    if (code) {
      console.error('wrk failed with ' + code);
      process.exit(code);
    }
    var match = out.match(regexp);
    var qps = match && +match[1];
    if (!qps) {
      console.error('%j', out);
      console.error('wrk produced strange output');
      process.exit(1);
    }
    self.report(+qps);
  });
};

Benchmark.prototype._run = function() {
  if (this.config)
    return this.fn(this.config);

  // some options weren't set.
  // run with all combinations
  var main = require.main.filename;
  var options = this.options;

  var queue = Object.keys(options).reduce(function(set, key) {
    var vals = options[key];
    assert(Array.isArray(vals));

    // match each item in the set with each item in the list
    var newSet = new Array(set.length * vals.length);
    var j = 0;
    set.forEach(function(s) {
      vals.forEach(function(val) {
        newSet[j++] = s.concat(key + '=' + val);
      });
    });
    return newSet;
  }, [[main]]);

  // output csv heading
  if (outputFormat == 'csv')
    console.log('filename,' + Object.keys(options).join(',') + ',result');

  var node = process.execPath;
  var i = 0;
  function run() {
    var argv = queue[i++];
    if (!argv)
      return;
    argv = process.execArgv.concat(argv);
    var child = child_process.spawn(node, argv, { stdio: 'inherit' });
    child.on('close', function(code, signal) {
      if (code)
        console.error('child process exited with code ' + code);
      else
        run();
    });
  }
  run();
};

function parseOpts(options) {
  // verify that there's an option provided for each of the options
  // if they're not *all* specified, then we return null.
  var keys = Object.keys(options);
  var num = keys.length;
  var conf = {};
  for (var i = 2; i < process.argv.length; i++) {
    var match = process.argv[i].match(/^(.+)=(.+)$/);
    if (!match || !match[1] || !match[2] || !options[match[1]]) {
      return null;
    } else {
      conf[match[1]] = isFinite(match[2]) ? +match[2] : match[2];
      num--;
    }
  }
  // still go ahead and set whatever WAS set, if it was.
  if (num !== 0) {
    Object.keys(conf).forEach(function(k) {
      options[k] = [conf[k]];
    });
  }
  return num === 0 ? conf : null;
}

Benchmark.prototype.start = function() {
  if (this._started)
    throw new Error('Called start more than once in a single benchmark');

  this._started = true;
  this._start = process.hrtime();
};

Benchmark.prototype.end = function(operations) {
  var elapsed = process.hrtime(this._start);

  if (!this._started)
    throw new Error('called end without start');
  if (typeof operations !== 'number')
    throw new Error('called end() without specifying operation count');

  var time = elapsed[0] + elapsed[1] / 1e9;
  var rate = operations / time;
  this.report(rate);
};

Benchmark.prototype.report = function(value) {
  var heading = this.getHeading();

  if (outputFormat == 'default')
    console.log('%s: %s', heading, value.toFixed(5));
  else if (outputFormat == 'csv')
    console.log('%s,%s', heading, value.toFixed(5));

  process.exit(0);
};

Benchmark.prototype.getHeading = function() {
  var conf = this.config;

  if (outputFormat == 'default') {
    return this._name + ' ' + Object.keys(conf).map(function(key) {
      return key + '=' + conf[key];
    }).join(' ');
  } else if (outputFormat == 'csv') {
    return this._name + ',' + Object.keys(conf).map(function(key) {
      return conf[key];
    }).join(',');
  }
};
