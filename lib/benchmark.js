// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS

'use strict';

const child_process = require('child_process');
const util = require('util');
const path = require('path');
const { setTimeout } = require('timers');
const {
  ObjectKeys,
  StringPrototypeSlice,
  StringPrototypeRepeat,
  ArrayPrototypePush,
  ArrayPrototypeIndexOf,
  ArrayPrototypeSplice,
  ArrayPrototypeSlice,
  ArrayIsArray,
  ArrayPrototypeConcat,
  BigIntPrototypeToString,
  ObjectPrototypeHasOwnProperty: ObjectHasOwn,
  Number,
  NumberMAX_VALUE,
  NumberPrototypeToString,
  JSONStringify
} = primordials;

const http_benchmarkers = require('_http-benchmarkers');

const {
  codes: {
    ERR_BENCHMARK_START_CALLED_MORE_THAN_ONCE,
    ERR_BENCHMARK_END_CALLED_MORE_THAN_ONCE,
    ERR_BENCHMARK_NO_OPERATION_COUNT,
    ERR_BENCHMARK_INVALID_OPERATION_COUNT,
    ERR_BENCHMARK_START_NOT_CALLED,
    ERR_BENCHMARK_CLOCK_PRECISION,
    ERR_INVALID_ARG_TYPE
  }
} = require('internal/errors');

const {
  validateFunction,
  validateString,
  validateNumber,
  validateArray,
  validateBoolean,
  validateStringArray,
} = require('internal/validators');
const { validatePath } = require('internal/fs/utils');

class Benchmark {

  constructor(fn, benchmarkConfigs, options = {}) {

    validateFunction(fn);

    if (options.flags)
      validateArray(options.flags);

    // Used to make sure a benchmark only start a timer once
    this._started = false;

    // Indicate that the benchmark ended
    this._ended = false;

    // Holds process.hrtime value
    this._time = 0n;

    this.options = options;

    this.name = StringPrototypeSlice(process.mainModule.filename, process.mainModule.path.length + 1);

    // Execution arguments i.e. flags used to run the jobs
    this.flags = process.env.NODE_BENCHMARK_FLAGS ?
      process.env.NODE_BENCHMARK_FLAGS.split(/\s+/) :
      [];

    // Parse job-specific configuration from the command line arguments
    const argv = ArrayPrototypeSlice(process.argv, 2);

    const parsed_args = this._parseArgs(argv, benchmarkConfigs, options);

    this.options = parsed_args.cli;
    this.extra_options = parsed_args.extra;

    if (options.flags) {
      this.flags = ArrayPrototypeConcat(this.flags, options.flags);
    }

    // The configuration list as a queue of jobs
    this.queue = this._queue(this.options);

    // The configuration of the current job, head of the queue
    this.config = this.queue[0];
    process.nextTick(() => {
      if (ObjectHasOwn(process.env, 'NODE_RUN_BENCHMARK_FN')) {
        fn(this.config);
      } else {
        // _run will use fork() to create a new process for each configuration
        // combination.
        this._run();
      }
    });
  }

  /**
   * Runs the http benchmaker specified in `options.benchmarker`
   * @param {object} options
   * @param {string} options.path
   * @param {number} options.connections
   * @param {number} options.duration
   * @param {string} options.benchmarker
   * @param {Function} cb
   */
  http(options, cb) {
    const http_options = { ...options };
    http_options.benchmarker = http_options.benchmarker ||
      this.config.benchmarker ||
      this.extra_options.benchmarker ||
      http_benchmarkers.default_http_benchmarker;
    http_benchmarkers.run(
      http_options, (error, code, used_benchmarker, result, elapsed) => {
        if (cb) {
          cb(code);
        }
        if (error) {
          throw error;
        }
        this.config.benchmarker = used_benchmarker;
        this.report(result, elapsed);
      }
    );
  }

  _run() {

    if (process.send) {
      process.send({
        type: 'config',
        name: this.name,
        queueLength: this.queue.length,
      });
    }

    const recursive = (queueIndex) => {
      const config = this.queue[queueIndex];

      // Set NODE_RUN_BENCHMARK_FN to indicate that the child shouldn't
      // construct a configuration queue, but just execute the benchmark
      // function.
      const childEnv = {
        ...process.env,
        NODE_RUN_BENCHMARK_FN: ''
      };

      const childArgs = [];

      for (const key of ObjectKeys(config)) {
        ArrayPrototypePush(childArgs, `${key}=${config[key]}`);
      }

      for (const key of ObjectKeys(this.extra_options)) {
        ArrayPrototypePush(childArgs, `${key}=${this.extra_options[key]}`);
      }

      const child = child_process.fork(process.mainModule.filename, childArgs, {
        env: childEnv,
        execArgv: ArrayPrototypeConcat(this.flags, process.execArgv)
      });

      child.on('message', Benchmark.SendResult);
      child.on('close', (code) => {
        if (code) {
          process.exit(code);
        }

        if (queueIndex + 1 < this.queue.length) {
          recursive(queueIndex + 1);
        }

      });
    };

    recursive(0);
  }

  _parseArgs(argv, config, options) {
    const cliOptions = {};
    // Check for the test mode first.
    const testIndex = ArrayPrototypeIndexOf(argv, '--test');
    if (testIndex !== -1) {
      for (const key of ObjectKeys(config)) {
        const rawValue = config[key];
        let value = ArrayIsArray(rawValue) ? rawValue[0] : rawValue;
        // Set numbers to one by default to reduce the runtime.
        if (typeof value === 'number') {
          if (key === 'dur' || key === 'duration') {
            value = 0.05;
          } else if (value > 1) {
            value = 1;
          }
        }
        cliOptions[key] = [value];
      }
      // Override specific test options.
      if (options.test) {
        for (const key of ObjectKeys(options.test)) {
          const value = options.test[key];
          cliOptions[key] = ArrayIsArray(value) ? value : [value];
        }
      }
      ArrayPrototypeSplice(argv, testIndex, 1);
    } else {
      // Accept single values instead of arrays.
      for (const key of ObjectKeys(config)) {
        const value = config[key];
        if (!ArrayIsArray(value))
          config[key] = [value];
      }
    }

    const extraOptions = {};
    const validArgRE = /^(.+?)=([\s\S]*)$/;
    // Parse configuration arguments
    for (const arg of argv) {

      const match = arg.match(validArgRE);

      if (!match) {
        throw new ERR_INVALID_ARG_TYPE(`bad argument: ${arg}`);
      }

      const key = match[1];
      const value = match[2];

      if (ObjectHasOwn(config, key)) {
        if (!cliOptions[key])
          cliOptions[key] = [];
        ArrayPrototypePush(
          cliOptions[key],
          // Infer the type from the config object and parse accordingly
          typeof config[key][0] === 'number' ? +value : value
        );
      } else {
        extraOptions[key] = value;
      }
    }
    return { cli: { ...config, ...cliOptions }, extra: extraOptions };
  }

  _queue(options) {

    const queue = [];
    const keys = ObjectKeys(options);

    function recursive(keyIndex, prevConfig) {
      const key = keys[keyIndex];
      const values = options[key];
      for (const value of values) {
        try {
          validateString(value, 'value');
        } catch (error) {
          if (error.code !== 'ERR_INVALID_ARG_TYPE') {
            throw error;
          }
          validateNumber(value, 'value');
        }

        if (typeof value !== typeof values[0]) {
          // This is a requirement for being able to consistently and
          // predictably parse CLI provided configuration values.
          throw new ERR_INVALID_ARG_TYPE(`configuration "${key}" has mixed types`);
        }

        const currConfig = { [key]: value, ...prevConfig };

        if (keyIndex + 1 < keys.length) {
          recursive(keyIndex + 1, currConfig);
        } else {
          ArrayPrototypePush(queue, currConfig);
        }
      }
    }

    if (keys.length > 0) {
      recursive(0, {});
    } else {
      ArrayPrototypePush(queue, {});
    }
    return queue;
  }

  start() {
    if (this._started) {
      throw new ERR_BENCHMARK_START_CALLED_MORE_THAN_ONCE();
    }
    this._started = true;
    this._time = process.hrtime.bigint();
  }

  end(operations) {
    // Get elapsed time now and do error checking later for accuracy.
    const time = process.hrtime.bigint();

    if (!this._started) {
      throw new ERR_BENCHMARK_START_NOT_CALLED();
    }
    if (this._ended) {
      throw new ERR_BENCHMARK_END_CALLED_MORE_THAN_ONCE();
    }
    if (typeof operations !== 'number') {
      throw new ERR_BENCHMARK_NO_OPERATION_COUNT();
    }
    if (!process.env.NODEJS_BENCHMARK_ZERO_ALLOWED && operations <= 0) {
      throw new ERR_BENCHMARK_INVALID_OPERATION_COUNT();
    }

    this._ended = true;

    if (time === this._time) {
      if (!process.env.NODEJS_BENCHMARK_ZERO_ALLOWED)
        throw new ERR_BENCHMARK_CLOCK_PRECISION();
      // Avoid dividing by zero
      this.report(operations && NumberMAX_VALUE, 0n);
      return;
    }

    const elapsed = time - this._time;
    const rate = operations / (Number(elapsed) / 1e9);
    this.report(rate, elapsed);
  }

  report(rate, elapsed) {
    Benchmark.SendResult({
      name: this.name,
      conf: this.config,
      rate,
      time: this._nanoSecondsToString(elapsed),
      type: 'report',
    });
  }

  _nanoSecondsToString(bigint) {
    const str = BigIntPrototypeToString(bigint);
    const decimalPointIndex = str.length - 9;
    if (decimalPointIndex <= 0) {
      return `0.${StringPrototypeRepeat('0', -decimalPointIndex)}${str}`;
    }
    return `${StringPrototypeSlice(str, 0, decimalPointIndex)}.${StringPrototypeSlice(str, decimalPointIndex)}`;
  }

  static SendResult(data) {
    if (process.send) {
      // If forked, report by process send
      process.send(data, () => {
        if (ObjectHasOwn(process.env, 'NODE_RUN_BENCHMARK_FN')) {
          // If, for any reason, the process is unable to self close within
          // a second after completing, forcefully close it.
          setTimeout(() => {
            process.exit(0);
          }, 5000).unref();
        }
      });
    } else {
      // Otherwise report by stdout
      process.stdout.write(Benchmark.FormatResult(data));
    }
  }

  static FormatResult(data) {
    // Construct configuration string, " A=a, B=b, ..."
    let conf = '';
    for (const key of ObjectKeys(data.conf)) {
      conf += ` ${key}=${JSONStringify(data.conf[key])}`;
    }
    let rate = NumberPrototypeToString(data.rate).split('.');
    rate[0] = rate[0].replace(/(\d)(?=(?:\d\d\d)+(?!\d))/g, '$1,');
    rate = (rate[1] ? rate.join('.') : rate[0]);
    return `${data.name}${conf}: ${rate}\n`;
  }
}

module.exports = {
  HTTP_BENCHMARK_PORT: http_benchmarkers.HTTP_BENCHMARK_PORT,
  Benchmark,
  /**
   * Returns an instance of Benchmark
   * @param {Function} fn
   * @param {Object<any>} config
   * @param {{
   *   flags: Array<string>
   * }} options
   * @returns {Benchmark}
   */
  createBenchmark(fn, config, options) {
    return new Benchmark(fn, config, options);
  },

  /**
   * Benchmarks an operation using
   * `options.binary.new` and `options.binary.old` and
   * compares the result.
   * @param {object}        options
   * @param {number}        options.runs
   * @param {boolean}       options.showProgress
   * @param {Array<string>} options.benchmarkFiles
   * @param {object}        options.binary
   * @param {string}        options.binary.old
   * @param {string}        options.binary.new
   * @param {Array<string>} options.set
   */
  compareBenchmarks(options) {
    const kStartOfQueue = 0;

    validateStringArray(options.benchmarkFiles);
    validateBoolean(options.showProgress);
    validatePath(options.binary.old);
    validatePath(options.binary.new);

    options.runs = typeof options.runs !== 'number' ? 30 : 0;

    validateNumber(options.runs);

    for (const file of options.benchmarkFiles) {
      validatePath(file);
    }

    if (options.set) {
      validateStringArray(options.set);
      for (const variable of options.set) {
        const tokens = variable.split('=');
        if (
          tokens.length > 2 ||
            tokens < 2 ||
          /^\s+$/.test(tokens[1])
        ) {
          throw new ERR_INVALID_ARG_TYPE(
            `Expected ${variable} to only follow this format key=value`
          );
        }
      }
    }

    const queue = BenchmarkProgress.QueueBenchmarkFiles(
      {
        binaries: ObjectKeys(options.binary),
        benchmarkFiles: options.benchmarkFiles,
        runs: options.runs,
      }
    );

    let progress;

    if (options.showProgress) {
      progress = new BenchmarkProgress(queue, options.benchmarkFiles);
      progress.startQueue(kStartOfQueue);
    }

    process.stdout.write('"binary","filename","configuration","rate","time"\n');

    (function recursive(i) {
      const job = queue[i];

      const child = child_process.fork(path.resolve(process.mainModule.path, job.filename), options.set, {
        execPath: options.binary[job.binary]
      });

      child.on('message', (data) => {
        if (data.type === 'report') {
          // Construct configuration string, " A=a, B=b, ..."
          let conf = '';
          for (const key of ObjectKeys(data.conf)) {
            conf += ` ${key}=${util.inspect(data.conf[key])}`;
          }
          conf = StringPrototypeSlice(conf, 1);
          // Escape quotes (") for correct csv formatting
          conf = conf.replace(/"/g, '""');

          process.stdout.write(
            `"${job.binary}","${job.filename}","${conf}",` +
              `${data.rate},${data.time}\n`
          );

          if (options.showProgress) {
            // One item in the subqueue has been completed.
            progress.completeConfig(data);
          }
        } else if (options.showProgress && data.type === 'config') {
          // The child has computed the configurations, ready to run subqueue.
          progress.startSubqueue(data, i);
        }
      });

      child.once('close', (code) => {
        if (code) {
          process.exit(code);
        }
        if (options.showProgress) {
          progress.completeRun(job);
        }
        // If there are more benchmarks execute the next
        if (i + 1 < queue.length) {
          recursive(i + 1);
        }
      });
    })(kStartOfQueue);
  }
};
