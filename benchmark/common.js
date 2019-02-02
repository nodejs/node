'use strict';

const child_process = require('child_process');
const http_benchmarkers = require('./_http-benchmarkers.js');

exports.buildType = process.features.debug ? 'Debug' : 'Release';

exports.createBenchmark = function(fn, configs, options) {
  return new Benchmark(fn, configs, options);
};

function Benchmark(fn, configs, options) {
  // Use the file name as the name of the benchmark
  this.name = require.main.filename.slice(__dirname.length + 1);
  // Parse job-specific configuration from the command line arguments
  const parsed_args = this._parseArgs(process.argv.slice(2), configs);
  this.options = parsed_args.cli;
  this.extra_options = parsed_args.extra;
  // The configuration list as a queue of jobs
  this.queue = this._queue(this.options);
  // The configuration of the current job, head of the queue
  this.config = this.queue[0];
  // Execution arguments i.e. flags used to run the jobs
  this.flags = [];
  if (options && options.flags) {
    this.flags = this.flags.concat(options.flags);
  }
  // Holds process.hrtime value
  this._time = [0, 0];
  // Used to make sure a benchmark only start a timer once
  this._started = false;

  // this._run will use fork() to create a new process for each configuration
  // combination.
  if (process.env.hasOwnProperty('NODE_RUN_BENCHMARK_FN')) {
    process.nextTick(() => fn(this.config));
  } else {
    process.nextTick(() => this._run());
  }
}

Benchmark.prototype._parseArgs = function(argv, configs) {
  const cliOptions = {};
  const extraOptions = {};
  const validArgRE = /^(.+?)=([\s\S]*)$/;
  // Parse configuration arguments
  for (const arg of argv) {
    const match = arg.match(validArgRE);
    if (!match) {
      console.error(`bad argument: ${arg}`);
      process.exit(1);
    }
    const config = match[1];

    if (configs[config]) {
      // Infer the type from the config object and parse accordingly
      const isNumber = typeof configs[config][0] === 'number';
      const value = isNumber ? +match[2] : match[2];
      if (!cliOptions[config])
        cliOptions[config] = [];
      cliOptions[config].push(value);
    } else {
      extraOptions[config] = match[2];
    }
  }
  return { cli: Object.assign({}, configs, cliOptions), extra: extraOptions };
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
exports.default_http_benchmarker =
  http_benchmarkers.default_http_benchmarker;
exports.PORT = http_benchmarkers.PORT;

Benchmark.prototype.http = function(options, cb) {
  const self = this;
  const http_options = Object.assign({ }, options);
  http_options.benchmarker = http_options.benchmarker ||
                             self.config.benchmarker ||
                             self.extra_options.benchmarker ||
                             exports.default_http_benchmarker;
  http_benchmarkers.run(
    http_options, (error, code, used_benchmarker, result, elapsed) => {
      if (cb) {
        cb(code);
      }
      if (error) {
        console.error(error);
        process.exit(code || 1);
      }
      self.config.benchmarker = used_benchmarker;
      self.report(result, elapsed);
    }
  );
};

Benchmark.prototype._run = function() {
  const self = this;
  // If forked, report to the parent.
  if (process.send) {
    process.send({
      type: 'config',
      name: this.name,
      queueLength: this.queue.length,
    });
  }

  (function recursive(queueIndex) {
    const config = self.queue[queueIndex];

    // Set NODE_RUN_BENCHMARK_FN to indicate that the child shouldn't construct
    // a configuration queue, but just execute the benchmark function.
    const childEnv = Object.assign({}, process.env);
    childEnv.NODE_RUN_BENCHMARK_FN = '';

    // Create configuration arguments
    const childArgs = [];
    for (const key of Object.keys(config)) {
      childArgs.push(`${key}=${config[key]}`);
    }
    for (const key of Object.keys(self.extra_options)) {
      childArgs.push(`${key}=${self.extra_options[key]}`);
    }

    const child = child_process.fork(require.main.filename, childArgs, {
      env: childEnv,
      execArgv: self.flags.concat(process.execArgv),
    });
    child.on('message', sendResult);
    child.on('close', (code) => {
      if (code) {
        process.exit(code);
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
  // Get elapsed time now and do error checking later for accuracy.
  const elapsed = process.hrtime(this._time);

  if (!this._started) {
    throw new Error('called end without start');
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
    // avoid dividing by zero
    elapsed[1] = 1;
  }

  const time = elapsed[0] + elapsed[1] / 1e9;
  const rate = operations / time;
  this.report(rate, elapsed);
};

function formatResult(data) {
  // Construct configuration string, " A=a, B=b, ..."
  let conf = '';
  for (const key of Object.keys(data.conf)) {
    conf += ` ${key}=${JSON.stringify(data.conf[key])}`;
  }

  var rate = data.rate.toString().split('.');
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
exports.sendResult = sendResult;

Benchmark.prototype.report = function(rate, elapsed) {
  sendResult({
    name: this.name,
    conf: this.config,
    rate: rate,
    time: elapsed[0] + elapsed[1] / 1e9,
    type: 'report',
  });
};

exports.binding = function(bindingName) {
  try {
    const { internalBinding } = require('internal/test/binding');

    return internalBinding(bindingName);
  } catch {
    return process.binding(bindingName);
  }
};

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
exports.urls = urls;

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
exports.searchParams = searchParams;

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

exports.urlDataTypes = Object.keys(urls).concat(['wpt']);

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
exports.bakeUrlData = bakeUrlData;
