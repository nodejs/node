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

const {
  codes: {
    ERR_BENCHMARK_HTTP_COULD_NOT_LOCATE_BENCHMARKER,
    ERR_BENCHMARK_HTTP_BENCHMARKER_NOT_INSTALLED,
    ERR_BENCHMARK_HTTP_BENCHMARKER_NOT_SUPPROTED,
    ERR_BENCHMARK_HTTP_STRANGE_OUTPUT,
    ERR_BENCHMARK_HTTP_UNKNOWN_OUTPUT,
  }
} = require('internal/errors');

const {
  Number,
  JSONParse,
  MathMax,
  MathMin,
  ArrayPrototypePush,
  NumberIsFinite,
  RegExpPrototypeExec
} = primordials;

const requirementsURL =
  'https://github.com/nodejs/node/blob/HEAD/benchmark/writing-and-running-benchmarks.md#http-benchmark-requirements';

// The port used by servers and wrk
exports.PORT = Number(process.env.PORT) || 12346;

class LoadBalancerBenchmarker {
  constructor(name, executable, type) {
    this.name = name;
    this.executable = executable;
    this.type = type;
    const result = child_process.spawnSync(this.executable, ['-h']);
    this.present = !(result.error && result.error.code === 'ENOENT');
  }
}

class AutocannonBenchmarker extends LoadBalancerBenchmarker {
  constructor() {
    super('autocannon', process.platform === 'win32' ? 'autocannon.cmd' : 'autocannon');
  }

  create(options) {
    const args = [
      '-d', options.duration,
      '-c', options.connections,
      '-j',
      '-n',
    ];
    for (const field in options.headers) {
      ArrayPrototypePush(args, '-H', `${field}=${options.headers[field]}`);
    }
    const scheme = options.scheme || 'http';
    ArrayPrototypePush(args, `${scheme}://127.0.0.1:${options.port}${options.path}`);
    const child = child_process.spawn(this.executable, args);
    return child;
  }

  processResults(output) {
    let result;
    try {
      result = JSONParse(output);
    } catch {
      return undefined;
    }
    if (!result || !result.requests || !result.requests.average) {
      return undefined;
    }
    return result.requests.average;
  }
}

class WrkBenchmarker extends LoadBalancerBenchmarker {
  constructor() {
    super('wrk', 'wrk');
  }

  create(options) {
    const duration = typeof options.duration === 'number' ?
      MathMax(options.duration, 1) :
      options.duration;
    const scheme = options.scheme || 'http';
    const args = [
      '-d', duration,
      '-c', options.connections,
      '-t', MathMin(options.connections, require('os').cpus().length || 8),
      `${scheme}://127.0.0.1:${options.port}${options.path}`,
    ];
    for (const field in options.headers) {
      ArrayPrototypePush(args, '-H', `${field}: ${options.headers[field]}`);
    }
    const child = child_process.spawn(this.executable, args);
    return child;
  }

  processResults(output) {
    const throughputRe = /Requests\/sec:[ \t]+([0-9.]+)/;
    const match = output.match(throughputRe);
    const throughput = match && +match[1];
    if (!NumberIsFinite(throughput)) {
      return undefined;
    }
    return throughput;
  }
}

/**
 * HTTP/2 Benchmarker
 */
class H2LoadBenchmarker extends LoadBalancerBenchmarker {
  constructor() {
    super('h2load', 'h2load');
  }

  create(options) {
    const args = [];

    if (typeof options.requests === 'number')
      ArrayPrototypePush(args, '-n', options.requests);
    if (typeof options.clients === 'number')
      ArrayPrototypePush(args, '-c', options.clients);
    if (typeof options.threads === 'number')
      ArrayPrototypePush(args, '-t', options.threads);
    if (typeof options.maxConcurrentStreams === 'number')
      ArrayPrototypePush(args, '-m', options.maxConcurrentStreams);
    if (typeof options.initialWindowSize === 'number')
      ArrayPrototypePush(args, '-w', options.initialWindowSize);
    if (typeof options.sessionInitialWindowSize === 'number')
      ArrayPrototypePush(args, '-W', options.sessionInitialWindowSize);
    if (typeof options.rate === 'number')
      ArrayPrototypePush(args, '-r', options.rate);
    if (typeof options.ratePeriod === 'number')
      ArrayPrototypePush(args, `--rate-period=${options.ratePeriod}`);
    if (typeof options.duration === 'number')
      ArrayPrototypePush(args, '-T', options.duration);
    if (typeof options.timeout === 'number')
      ArrayPrototypePush(args, '-N', options.timeout);
    if (typeof options.headerTableSize === 'number')
      ArrayPrototypePush(args, `--header-table-size=${options.headerTableSize}`);
    if (typeof options.encoderHeaderTableSize === 'number') {
      ArrayPrototypePush(args, `--encoder-header-table-size=${options.encoderHeaderTableSize}`);
    }
    const scheme = options.scheme || 'http';
    const host = options.host || '127.0.0.1';
    ArrayPrototypePush(args, `${scheme}://${host}:${options.port}${options.path}`);
    const child = child_process.spawn(this.executable, args);
    return child;
  }

  processResults(output) {
    const rex = /(\d+\.\d+) req\/s/;
    return RegExpPrototypeExec(rex, output)[1];
  }
}

const http_benchmarkers = [
  new WrkBenchmarker(),
  new AutocannonBenchmarker(),
  new H2LoadBenchmarker(),
];

const benchmarkers = {};

http_benchmarkers.forEach((benchmarker) => {
  benchmarkers[benchmarker.name] = benchmarker;
  if (!exports.default_http_benchmarker && benchmarker.present) {
    exports.default_http_benchmarker = benchmarker.name;
  }
});

exports.run = function(options, callback) {
  options = {
    port: exports.PORT,
    path: '/',
    connections: 100,
    duration: 5,
    host: '127.0.0.1',
    benchmarker: exports.default_http_benchmarker,
    ...options
  };
  let err;
  if (!options.benchmarker) {
    err = new ERR_BENCHMARK_HTTP_COULD_NOT_LOCATE_BENCHMARKER(
      'Could not locate required http benchmarker. See ' +
        `${requirementsURL} for further instructions.`
    );
    callback(err);
    return;
  }
  const benchmarker = benchmarkers[options.benchmarker];
  if (!benchmarker) {
    err = new ERR_BENCHMARK_HTTP_BENCHMARKER_NOT_SUPPROTED(
      `Requested benchmarker '${options.benchmarker}' ` +
        'is not supported'
    );
    callback(err);
    return;
  }
  if (!benchmarker.present) {
    err = new ERR_BENCHMARK_HTTP_BENCHMARKER_NOT_INSTALLED(
      `Requested benchmarker '${options.benchmarker}' ` +
        'is  not installed'
    );
    callback(err);
    return;
  }

  const benchmarker_start = process.hrtime.bigint();

  const child = benchmarker.create(options);

  child.stderr.pipe(process.stderr);

  let stdout = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => stdout += chunk);

  child.once('close', (code) => {
    const benchmark_end = process.hrtime.bigint();
    if (code) {
      let error_message = `${options.benchmarker} failed with ${code}.`;
      if (stdout !== '') {
        error_message += ` Output: ${stdout}`;
      }
      callback(new ERR_BENCHMARK_HTTP_UNKNOWN_OUTPUT(error_message), code);
      return;
    }

    const result = benchmarker.processResults(stdout);
    if (result === undefined) {
      callback(new ERR_BENCHMARK_HTTP_STRANGE_OUTPUT(
        `${options.benchmarker} produced strange output: ${stdout}`), code);
      return;
    }

    const elapsed = benchmark_end - benchmarker_start;
    callback(null, code, options.benchmarker, result, elapsed);
  });

};
