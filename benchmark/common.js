'use strict';

const child_process = require('child_process');
const http_benchmarkers = require('./_http-benchmarkers.js');

class Benchmark {
  // Used to make sure a benchmark only start a timer once
  #started = false;

  // Indicate that the benchmark ended
  #ended = false;

  // Holds process.hrtime value
  #time = [0, 0];

  // Use the file name as the name of the benchmark
  name = require.main.filename.slice(__dirname.length + 1);

  // Execution arguments i.e. flags used to run the jobs
  flags = process.env.NODE_BENCHMARK_FLAGS ?
    process.env.NODE_BENCHMARK_FLAGS.split(/\s+/) :
    [];

  constructor(fn, configs, options = {}) {
    // Parse job-specific configuration from the command line arguments
    const argv = process.argv.slice(2);
    const parsed_args = this._parseArgs(argv, configs, options);
    this.options = parsed_args.cli;
    this.extra_options = parsed_args.extra;
    if (options.flags) {
      this.flags = this.flags.concat(options.flags);
    }

    // The configuration list as a queue of jobs
    this.queue = this._queue(this.options);

    // The configuration of the current job, head of the queue
    this.config = this.queue[0];

    process.nextTick(() => {
      if (process.env.hasOwnProperty('NODE_RUN_BENCHMARK_FN')) {
        fn(this.config);
      } else {
        // _run will use fork() to create a new process for each configuration
        // combination.
        this._run();
      }
    });
  }

  _parseArgs(argv, configs, options) {
    const cliOptions = {};

    // Check for the test mode first.
    const testIndex = argv.indexOf('--test');
    if (testIndex !== -1) {
      for (const [key, rawValue] of Object.entries(configs)) {
        let value = Array.isArray(rawValue) ? rawValue[0] : rawValue;
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
        for (const [key, value] of Object.entries(options.test)) {
          cliOptions[key] = Array.isArray(value) ? value : [value];
        }
      }
      argv.splice(testIndex, 1);
    } else {
      // Accept single values instead of arrays.
      for (const [key, value] of Object.entries(configs)) {
        if (!Array.isArray(value))
          configs[key] = [value];
      }
    }

    const extraOptions = {};
    const validArgRE = /^(.+?)=([\s\S]*)$/;
    // Parse configuration arguments
    for (const arg of argv) {
      const match = arg.match(validArgRE);
      if (!match) {
        console.error(`bad argument: ${arg}`);
        process.exit(1);
      }
      const [, key, value] = match;
      if (Object.prototype.hasOwnProperty.call(configs, key)) {
        if (!cliOptions[key])
          cliOptions[key] = [];
        cliOptions[key].push(
          // Infer the type from the config object and parse accordingly
          typeof configs[key][0] === 'number' ? +value : value
        );
      } else {
        extraOptions[key] = value;
      }
    }
    return { cli: { ...configs, ...cliOptions }, extra: extraOptions };
  }

  _queue(options) {
    const queue = [];
    const keys = Object.keys(options);

    // Perform a depth-first walk through all options to generate a
    // configuration list that contains all combinations.
    function recursive(keyIndex, prevConfig) {
      const key = keys[keyIndex];
      const values = options[key];

      for (const value of values) {
        if (typeof value !== 'number' && typeof value !== 'string') {
          throw new TypeError(
            `configuration "${key}" had type ${typeof value}`);
        }
        if (typeof value !== typeof values[0]) {
          // This is a requirement for being able to consistently and
          // predictably parse CLI provided configuration values.
          throw new TypeError(`configuration "${key}" has mixed types`);
        }

        const currConfig = { [key]: value, ...prevConfig };

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
  }

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
          console.error(error);
          process.exit(code || 1);
        }
        this.config.benchmarker = used_benchmarker;
        this.report(result, elapsed);
      }
    );
  }

  _run() {
    // If forked, report to the parent.
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
      const childEnv = { ...process.env };
      childEnv.NODE_RUN_BENCHMARK_FN = '';

      // Create configuration arguments
      const childArgs = [];
      for (const [key, value] of Object.entries(config)) {
        childArgs.push(`${key}=${value}`);
      }
      for (const [key, value] of Object.entries(this.extra_options)) {
        childArgs.push(`${key}=${value}`);
      }

      const child = child_process.fork(require.main.filename, childArgs, {
        env: childEnv,
        execArgv: this.flags.concat(process.execArgv),
      });
      child.on('message', sendResult);
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

  start() {
    if (this.#started) {
      throw new Error('Called start more than once in a single benchmark');
    }
    this.#started = true;
    this.#time = process.hrtime();
  }

  end(operations) {
    // Get elapsed time now and do error checking later for accuracy.
    const elapsed = process.hrtime(this.#time);

    if (!this.#started) {
      throw new Error('called end without start');
    }
    if (this.#ended) {
      throw new Error('called end multiple times');
    }
    if (typeof operations !== 'number') {
      throw new Error('called end() without specifying operation count');
    }
    if (!process.env.NODEJS_BENCHMARK_ZERO_ALLOWED && operations <= 0) {
      throw new Error('called end() with operation count <= 0');
    }
    if (elapsed[0] === 0 && elapsed[1] === 0) {
      if (!process.env.NODEJS_BENCHMARK_ZERO_ALLOWED)
        throw new Error('insufficient clock precision for short benchmark');
      // Avoid dividing by zero
      elapsed[1] = 1;
    }

    this.#ended = true;
    const time = elapsed[0] + elapsed[1] / 1e9;
    const rate = operations / time;
    this.report(rate, elapsed);
  }

  report(rate, elapsed) {
    sendResult({
      name: this.name,
      conf: this.config,
      rate,
      time: elapsed[0] + elapsed[1] / 1e9,
      type: 'report',
    });
  }
}

function formatResult(data) {
  // Construct configuration string, " A=a, B=b, ..."
  let conf = '';
  for (const key of Object.keys(data.conf)) {
    conf += ` ${key}=${JSON.stringify(data.conf[key])}`;
  }

  let rate = data.rate.toString().split('.');
  rate[0] = rate[0].replace(/(\d)(?=(?:\d\d\d)+(?!\d))/g, '$1,');
  rate = (rate[1] ? rate.join('.') : rate[0]);
  return `${data.name}${conf}: ${rate}`;
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

const urls = {
  long: 'http://nodejs.org:89/docs/latest/api/foo/bar/qua/13949281/0f28b/' +
        '/5d49/b3020/url.html#test?payload1=true&payload2=false&test=1' +
        '&benchmark=3&foo=38.38.011.293&bar=1234834910480&test=19299&3992&' +
        'key=f5c65e1e98fe07e648249ad41e1cfdb0',
  short: 'https://nodejs.org/en/blog/',
  idn: 'http://你好你好.在线',
  auth: 'https://user:pass@example.com/path?search=1',
  file: 'file:///foo/bar/test/node.js',
  ws: 'ws://localhost:9229/f46db715-70df-43ad-a359-7f9949f39868',
  javascript: 'javascript:alert("node is awesome");',
  percent: 'https://%E4%BD%A0/foo',
  dot: 'https://example.org/./a/../b/./c',
};

const searchParams = {
  noencode: 'foo=bar&baz=quux&xyzzy=thud',
  multicharsep: 'foo=bar&&&&&&&&&&baz=quux&&&&&&&&&&xyzzy=thud',
  encodefake: 'foo=%©ar&baz=%A©uux&xyzzy=%©ud',
  encodemany: '%66%6F%6F=bar&%62%61%7A=quux&xyzzy=%74h%75d',
  encodelast: 'foo=bar&baz=quux&xyzzy=thu%64',
  multivalue: 'foo=bar&foo=baz&foo=quux&quuy=quuz',
  multivaluemany: 'foo=bar&foo=baz&foo=quux&quuy=quuz&foo=abc&foo=def&' +
                  'foo=ghi&foo=jkl&foo=mno&foo=pqr&foo=stu&foo=vwxyz',
  manypairs: 'a&b&c&d&e&f&g&h&i&j&k&l&m&n&o&p&q&r&s&t&u&v&w&x&y&z',
  manyblankpairs: '&&&&&&&&&&&&&&&&&&&&&&&&',
  altspaces: 'foo+bar=baz+quux&xyzzy+thud=quuy+quuz&abc=def+ghi',
};

function getUrlData(withBase) {
  const data = require('../test/fixtures/wpt/url/resources/urltestdata.json');
  const result = [];
  for (const item of data) {
    if (item.failure || !item.input) continue;
    if (withBase) {
      result.push([item.input, item.base]);
    } else if (item.base !== 'about:blank') {
      result.push(item.base);
    }
  }
  return result;
}

/**
 * Generate an array of data for URL benchmarks to use.
 * The size of the resulting data set is the original data size * 2 ** `e`.
 * The 'wpt' type contains about 400 data points when `withBase` is true,
 * and 200 data points when `withBase` is false.
 * Other types contain 200 data points with or without base.
 *
 * @param {string} type Type of the data, 'wpt' or a key of `urls`
 * @param {number} e The repetition of the data, as exponent of 2
 * @param {boolean} withBase Whether to include a base URL
 * @param {boolean} asUrl Whether to return the results as URL objects
 * @return {string[] | string[][] | URL[]}
 */
function bakeUrlData(type, e = 0, withBase = false, asUrl = false) {
  let result = [];
  if (type === 'wpt') {
    result = getUrlData(withBase);
  } else if (urls[type]) {
    const input = urls[type];
    const item = withBase ? [input, 'about:blank'] : input;
    // Roughly the size of WPT URL test data
    result = new Array(200).fill(item);
  } else {
    throw new Error(`Unknown url data type ${type}`);
  }

  if (typeof e !== 'number') {
    throw new Error(`e must be a number, received ${e}`);
  }

  for (let i = 0; i < e; ++i) {
    result = result.concat(result);
  }

  if (asUrl) {
    if (withBase) {
      result = result.map(([input, base]) => new URL(input, base));
    } else {
      result = result.map((input) => new URL(input));
    }
  }
  return result;
}

module.exports = {
  Benchmark,
  PORT: http_benchmarkers.PORT,
  bakeUrlData,
  binding(bindingName) {
    try {
      const { internalBinding } = require('internal/test/binding');

      return internalBinding(bindingName);
    } catch {
      return process.binding(bindingName);
    }
  },
  buildType: process.features.debug ? 'Debug' : 'Release',
  createBenchmark(fn, configs, options) {
    return new Benchmark(fn, configs, options);
  },
  sendResult,
  searchParams,
  urlDataTypes: Object.keys(urls).concat(['wpt']),
  urls,
};
