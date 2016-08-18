'use strict';

const child_process = require('child_process');
const HTTPBenchmarkers = require('./http-benchmarkers.js').HTTPBenchmarkers;

// The port used by servers and wrk
exports.PORT = process.env.PORT || 12346;

exports.createBenchmark = function(fn, options) {
  return new Benchmark(fn, options);
};

function Benchmark(fn, options) {
  this.name = require.main.filename.slice(__dirname.length + 1);
  this.options = this._parseArgs(process.argv.slice(2), options);
  this.queue = this._queue(this.options);
  this.config = this.queue[0];

  this._time = [0, 0]; // holds process.hrtime value
  this._started = false;

  // this._run will use fork() to create a new process for each configuration
  // combination.
  if (process.env.hasOwnProperty('NODE_RUN_BENCHMARK_FN')) {
    process.nextTick(() => fn(this.config));
  } else {
    process.nextTick(() => this._run());
  }
}

Benchmark.prototype._parseArgs = function(argv, options) {
  const cliOptions = Object.assign({}, options);

  // Parse configuration arguments
  for (const arg of argv) {
    const match = arg.match(/^(.+?)=([\s\S]*)$/);
    if (!match || !match[1]) {
      console.error('bad argument: ' + arg);
      process.exit(1);
    }

    // Infer the type from the options object and parse accordingly
    const isNumber = typeof options[match[1]][0] === 'number';
    const value = isNumber ? +match[2] : match[2];

    cliOptions[match[1]] = [value];
  }

  return cliOptions;
};

Benchmark.prototype._queue = function(options) {
  const queue = [];
  const keys = Object.keys(options);

  // Perform a depth-first walk though all options to generate a
  // configuration list that contains all combinations.
  function recursive(keyIndex, prevConfig) {
    const key = keys[keyIndex];
    const values = options[key];
    const type = typeof values[0];

    for (const value of values) {
      if (typeof value !== 'number' && typeof value !== 'string') {
        throw new TypeError(`configuration "${key}" had type ${typeof value}`);
      }
      if (typeof value !== type) {
        // This is a requirement for being able to consistently and predictably
        // parse CLI provided configuration values.
        throw new TypeError(`configuration "${key}" has mixed types`);
      }

      const currConfig = Object.assign({ [key]: value }, prevConfig);

      if (keyIndex + 1 < keys.length) {
        recursive(keyIndex + 1, currConfig);
      } else {
        queue.push(currConfig);
      }
    }
  }

  if (keys.length > 0) {
    recursive(0, {});
  } else {
    queue.push({});
  }

  return queue;
};

// Benchmark an http server.
Benchmark.prototype.http = function(urlPath, duration, connections, cb) {
  const self = this;
  duration = 1;

  const picked_benchmarker = process.env.NODE_HTTP_BENCHMARKER ||
                             this.config.benchmarker || 'all';
  const benchmarkers = picked_benchmarker === 'all'
                     ? Object.keys(HTTPBenchmarkers)
                     : [picked_benchmarker];

  // See if any benchmarker is available. Also test if all used benchmarkers
  // are defined
  var any_available = false;
  for (var i = 0; i < benchmarkers.length; ++i) {
    const benchmarker = benchmarkers[i];
    const http_benchmarker = HTTPBenchmarkers[benchmarker];
    if (http_benchmarker === undefined) {
      console.error(`Unknown http benchmarker: ${benchmarker}`);
      process.exit(1);
    }
    if (http_benchmarker.present()) {
      any_available = true;
    }
  }
  if (!any_available) {
    console.error('Could not locate any of the required http benchmarkers' +
                  `(${benchmarkers.join(', ')}). Check ` +
                  'benchmark/README.md for further instructions.');
    process.exit(1);
  }

  function runHttpBenchmarker(index, collected_code) {
    // All benchmarkers executed
    if (index === benchmarkers.length) {
      if (cb)
        cb(collected_code);
      if (collected_code !== 0)
        process.exit(1);
      return;
    }

    // Run next benchmarker
    const benchmarker = benchmarkers[index];
    self.config.benchmarker = benchmarker;

    const http_benchmarker = HTTPBenchmarkers[benchmarker];
    if (http_benchmarker.present()) {
      const child_start = process.hrtime();
      var child = http_benchmarker.create(exports.PORT, urlPath, duration,
                                          connections);

      // Collect stdout
      let stdout = '';
      child.stdout.on('data', (chunk) => stdout += chunk.toString());

      child.once('close', function(code) {
        const elapsed = process.hrtime(child_start);
        if (code) {
          if (stdout === '') {
            console.error(`${benchmarker} failed with ${code}`);
          } else {
            console.error(`${benchmarker} failed with ${code}. Output:`);
            console.error(stdout);
          }
          runHttpBenchmarker(index + 1, code);
          return;
        }

        var result = http_benchmarker.processResults(stdout);
        if (!result) {
          console.error(`${benchmarker} produced strange output`);
          console.error(stdout);
          runHttpBenchmarker(index + 1, 1);
          return;
        }

        self.report(result, elapsed);
        runHttpBenchmarker(index + 1, collected_code);
      });
    } else {
      runHttpBenchmarker(index + 1, collected_code);
    }
  }

  // Run with all benchmarkers
  runHttpBenchmarker(0, 0);
};

Benchmark.prototype._run = function() {
  const self = this;

  (function recursive(queueIndex) {
    const config = self.queue[queueIndex];

    // set NODE_RUN_BENCHMARK_FN to indicate that the child shouldn't construct
    // a configuration queue, but just execute the benchmark function.
    const childEnv = Object.assign({}, process.env);
    childEnv.NODE_RUN_BENCHMARK_FN = '';

    // Create configuration arguments
    const childArgs = [];
    for (const key of Object.keys(config)) {
      childArgs.push(`${key}=${config[key]}`);
    }

    const child = child_process.fork(require.main.filename, childArgs, {
      env: childEnv
    });
    child.on('message', sendResult);
    child.on('close', function(code) {
      if (code) {
        process.exit(code);
        return;
      }

      if (queueIndex + 1 < self.queue.length) {
        recursive(queueIndex + 1);
      }
    });
  })(0);
};

Benchmark.prototype.start = function() {
  if (this._started) {
    throw new Error('Called start more than once in a single benchmark');
  }
  this._started = true;
  this._time = process.hrtime();
};

Benchmark.prototype.end = function(operations) {
  // get elapsed time now and do error checking later for accuracy.
  const elapsed = process.hrtime(this._time);

  if (!this._started) {
    throw new Error('called end without start');
  }
  if (typeof operations !== 'number') {
    throw new Error('called end() without specifying operation count');
  }

  const time = elapsed[0] + elapsed[1] / 1e9;
  const rate = operations / time;
  this.report(rate, elapsed);
};

function formatResult(data) {
  // Construct configuration string, " A=a, B=b, ..."
  let conf = '';
  for (const key of Object.keys(data.conf)) {
    conf += ' ' + key + '=' + JSON.stringify(data.conf[key]);
  }

  return `${data.name}${conf}: ${data.rate}`;
}

function sendResult(data) {
  if (process.send) {
    // If forked, report by process send
    process.send(data);
  } else {
    // Otherwise report by stdout
    console.log(formatResult(data));
  }
}
exports.sendResult = sendResult;

Benchmark.prototype.report = function(rate, elapsed) {
  sendResult({
    name: this.name,
    conf: this.config,
    rate: rate,
    time: elapsed[0] + elapsed[1] / 1e9
  });
};

exports.v8ForceOptimization = function(method, ...args) {
  if (typeof method !== 'function')
    return;
  const v8 = require('v8');
  v8.setFlagsFromString('--allow_natives_syntax');
  method.apply(null, args);
  eval('%OptimizeFunctionOnNextCall(method)');
  method.apply(null, args);
  return eval('%GetOptimizationStatus(method)');
};
