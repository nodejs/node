'use strict';

const child_process = require('child_process');

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

  // Parse configuarion arguments
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

  // Perform a depth-first walk though all options to genereate a
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

function hasWrk() {
  const result = child_process.spawnSync('wrk', ['-h']);
  if (result.error && result.error.code === 'ENOENT') {
    console.error('Couldn\'t locate `wrk` which is needed for running ' +
      'benchmarks. Check benchmark/README.md for further instructions.');
    process.exit(1);
  }
}

// benchmark an http server.
const WRK_REGEXP = /Requests\/sec:[ \t]+([0-9\.]+)/;
Benchmark.prototype.http = function(urlPath, args, cb) {
  hasWrk();
  const self = this;

  const urlFull = 'http://127.0.0.1:' + exports.PORT + urlPath;
  args = args.concat(urlFull);

  const childStart = process.hrtime();
  const child = child_process.spawn('wrk', args);
  child.stderr.pipe(process.stderr);

  // Collect stdout
  let stdout = '';
  child.stdout.on('data', (chunk) => stdout += chunk.toString());

  child.once('close', function(code) {
    const elapsed = process.hrtime(childStart);
    if (cb) cb(code);

    if (code) {
      console.error('wrk failed with ' + code);
      process.exit(code);
    }

    // Extract requests pr second and check for odd results
    const match = stdout.match(WRK_REGEXP);
    if (!match || match.length <= 1) {
      console.error('wrk produced strange output:');
      console.error(stdout);
      process.exit(1);
    }

    // Report rate
    self.report(+match[1], elapsed);
  });
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
  if (this._started)
    throw new Error('Called start more than once in a single benchmark');

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
  // Construct confiuration string, " A=a, B=b, ..."
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
