'use strict';

const child_process = require('child_process');

function AutocannonBenchmarker() {
  this.name = 'autocannon';

  const autocannon_exe = process.platform === 'win32'
                         ? 'autocannon.cmd'
                         : 'autocannon';
  this.present = function() {
    var result = child_process.spawnSync(autocannon_exe, ['-h']);
    return !(result.error && result.error.code === 'ENOENT');
  };

  this.create = function(port, path, duration, connections) {
    const args = ['-d', duration, '-c', connections, '-j', '-n',
                  `http://127.0.0.1:${port}${path}` ];
    var child = child_process.spawn(autocannon_exe, args);
    child.stdout.setEncoding('utf8');
    return child;
  };

  this.processResults = function(output) {
    let result;
    try {
      result = JSON.parse(output);
    } catch (err) {
      // Do nothing, let next line handle this
    }
    if (!result || !result.requests || !result.requests.average) {
      return undefined;
    } else {
      return result.requests.average;
    }
  };
}

function WrkBenchmarker() {
  this.name = 'wrk';

  this.present = function() {
    var result = child_process.spawnSync('wrk', ['-h']);
    return !(result.error && result.error.code === 'ENOENT');
  };

  this.create = function(port, path, duration, connections) {
    const args = ['-d', duration, '-c', connections, '-t', 8,
                  `http://127.0.0.1:${port}${path}` ];
    var child = child_process.spawn('wrk', args);
    child.stdout.setEncoding('utf8');
    child.stderr.pipe(process.stderr);
    return child;
  };

  const regexp = /Requests\/sec:[ \t]+([0-9\.]+)/;
  this.processResults = function(output) {
    const match = output.match(regexp);
    const result = match && +match[1];
    if (!result) {
      return undefined;
    } else {
      return result;
    }
  };
}

const http_benchmarkers = [ new AutocannonBenchmarker(),
                            new WrkBenchmarker() ];

var default_http_benchmarker;
var supported_http_benchmarkers = [];
var installed_http_benchmarkers = [];
var benchmarkers = {};

http_benchmarkers.forEach((benchmarker) => {
  const name = benchmarker.name;
  const present = benchmarker.present();
  benchmarkers[name] = {
    benchmarker: benchmarker,
    present: present,
    default: false
  };

  supported_http_benchmarkers.push(name);
  if (present) {
    if (!default_http_benchmarker) {
      default_http_benchmarker = name;
      benchmarkers[name].default = true;
    }
    installed_http_benchmarkers.push(name);
  }
});

function getBenchmarker(name) {
  const benchmarker = benchmarkers[name];
  if (!benchmarker) {
    throw new Error(`benchmarker '${name}' is not supported`);
  }
  if (!benchmarker.present) {
    throw new Error(`benchmarker '${name}' is not installed`);
  }
  return benchmarker.benchmarker;
}

if (process.env.NODE_HTTP_BENCHMARKER) {
  const requested = process.env.NODE_HTTP_BENCHMARKER;
  try {
    default_http_benchmarker = requested;
    getBenchmarker(requested);
  } catch (err) {
    console.error('Error when overriding default http benchmarker: ' +
                  err.message);
    process.exit(1);
  }
}

exports.run = function(options, callback) {
  options = Object.assign({
    port: 1234,
    path: '/',
    connections: 100,
    duration: 10,
    benchmarker: default_http_benchmarker
  }, options);
  if (!options.benchmarker) {
    console.error('Could not locate any of the required http benchmarkers' +
                  'Check benchmark/README.md for further instructions.');
    process.exit(1);
  }
  const benchmarker = getBenchmarker(options.benchmarker);

  const benchmarker_start = process.hrtime();

  var child = benchmarker.create(options.port, options.path, options.duration,
                                 options.connections);

  let stdout = '';
  child.stdout.on('data', (chunk) => stdout += chunk.toString());

  child.once('close', function(code) {
    const elapsed = process.hrtime(benchmarker_start);
    if (code) {
      if (stdout === '') {
        console.error(`${options.benchmarker} failed with ${code}`);
      } else {
        console.error(`${options.benchmarker} failed with ${code}. Output:`);
        console.error(stdout);
      }
      process.exit(1);
    }

    var result = benchmarker.processResults(stdout);
    if (!result) {
      console.error(`${options.benchmarker} produced strange output`);
      console.error(stdout);
      process.exit(1);
    }

    callback(options.benchmarker, result, elapsed);
  });

};

exports.default_http_benchmarker = default_http_benchmarker;
exports.supported_http_benchmarkers = supported_http_benchmarkers;
exports.installed_http_benchmarkers = installed_http_benchmarkers;

