var assert = require('assert');
var path = require('path');

exports.PORT = process.env.PORT || 12346;

exports.createBenchmark = function(fn, options) {
  return new Benchmark(fn, options);
};

function Benchmark(fn, options) {
  this.fn = fn;
  this.options = options;
  this.config = parseOpts(options);
  this._name = path.basename(require.main.filename, '.js');
  this._start = [0,0];
  this._started = false;
  var self = this;
  process.nextTick(function() {
    self._run();
  });
}

Benchmark.prototype._run = function() {
  if (this.config)
    return this.fn(this.config);

  // one more more options weren't set.
  // run with all combinations
  var main = require.main.filename;
  var settings = [];
  var queueLen = 1;
  var options = this.options;

  var queue = Object.keys(options).reduce(function(set, key) {
    var vals = options[key];
    assert(Array.isArray(vals));

    // match each item in the set with each item in the list
    var newSet = new Array(set.length * vals.length);
    var j = 0;
    set.forEach(function(s) {
      vals.forEach(function(val) {
        newSet[j++] = s.concat('--' + key + '=' + val);
      });
    });
    return newSet;
  }, [[main]]);

  var spawn = require('child_process').spawn;
  var node = process.execPath;
  var i = 0;
  function run() {
    var argv = queue[i++];
    if (!argv)
      return;
    var child = spawn(node, argv, { stdio: 'inherit' });
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
  // verify that there's an --option provided for each of the options
  // if they're not *all* specified, then we return null.
  var keys = Object.keys(options);
  var num = keys.length;
  var conf = {};
  for (var i = 2; i < process.argv.length; i++) {
    var m = process.argv[i].match(/^--(.+)=(.+)$/);
    if (!m || !m[1] || !m[2] || !options[m[1]])
      return null;
    else {
      conf[m[1]] = isFinite(m[2]) ? +m[2] : m[2]
      num--;
    }
  }
  return num === 0 ? conf : null;
};

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
  var time = elapsed[0] + elapsed[1]/1e9;
  var rate = operations/time;
  var heading = this.getHeading();
  console.log('%s: %s', heading, rate.toPrecision(5));
  process.exit(0);
};

Benchmark.prototype.getHeading = function() {
  var conf = this.config;
  return this._name + '_' + Object.keys(conf).map(function(key) {
    return key + '_' + conf[key];
  }).join('_');
}
