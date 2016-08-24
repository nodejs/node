'use strict';

const child_process = require('child_process');

// The port used by servers and wrk
exports.PORT = process.env.PORT || 12346;

function AutocannonBenchmarker() {
  this.name = 'autocannon';
  this.autocannon_exe = process.platform === 'win32'
                      ? 'autocannon.cmd'
                      : 'autocannon';
}

AutocannonBenchmarker.prototype.present = function() {
  const result = child_process.spawnSync(this.autocannon_exe, ['-h']);
  return !(result.error && result.error.code === 'ENOENT');
};

AutocannonBenchmarker.prototype.create = function(options) {
  const args = ['-d', options.duration, '-c', options.connections, '-j', '-n',
                `http://127.0.0.1:${options.port}${options.path}` ];
  const child = child_process.spawn(this.autocannon_exe, args);
  return child;
};

AutocannonBenchmarker.prototype.processResults = function(output) {
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

function WrkBenchmarker() {
  this.name = 'wrk';
  this.regexp = /Requests\/sec:[ \t]+([0-9\.]+)/;
}

WrkBenchmarker.prototype.present = function() {
  const result = child_process.spawnSync('wrk', ['-h']);
  return !(result.error && result.error.code === 'ENOENT');
};

WrkBenchmarker.prototype.create = function(options) {
  const args = ['-d', options.duration, '-c', options.connections, '-t', 8,
                `http://127.0.0.1:${options.port}${options.path}` ];
  const child = child_process.spawn('wrk', args);
  return child;
};

WrkBenchmarker.prototype.processResults = function(output) {
  const match = output.match(this.regexp);
  const result = match && +match[1];
  if (!result) {
    return undefined;
  } else {
    return result;
  }
};

const http_benchmarkers = [ new WrkBenchmarker(),
                            new AutocannonBenchmarker() ];

const supported_http_benchmarkers = [];
const installed_http_benchmarkers = [];
const benchmarkers = {};

http_benchmarkers.forEach((benchmarker) => {
  const name = benchmarker.name;
  const present = benchmarker.present();

  benchmarkers[name] = {
    instance: benchmarker,
    present: present
  };

  supported_http_benchmarkers.push(name);
  if (present) {
    installed_http_benchmarkers.push(name);
  }

});

let default_http_benchmarker;

if (process.env.NODE_HTTP_BENCHMARKER) {
  default_http_benchmarker = process.env.NODE_HTTP_BENCHMARKER;
  if (!benchmarkers[default_http_benchmarker]) {
    throw new Error('Requested default benchmarker ' +
                    `'${default_http_benchmarker}' is not supported`);
  }
  if (!benchmarkers[default_http_benchmarker].present) {
    throw new Error('Requested default benchmarker ' +
                    `'${default_http_benchmarker}' is not installed`);
  }
} else {
  default_http_benchmarker = installed_http_benchmarkers[0];
}

exports.run = function(options, callback) {
  options = Object.assign({
    port: exports.PORT,
    path: '/',
    connections: 100,
    duration: 10,
    benchmarker: default_http_benchmarker
  }, options);
  if (!options.benchmarker) {
    callback(new Error('Could not locate any of the required http' +
                       'benchmarkers. Check benchmark/README.md for further ' +
                       'instructions.'));
    return;
  }
  var benchmarker = benchmarkers[options.benchmarker];
  if (!benchmarker) {
    callback(new Error(`Requested benchmarker '${options.benchmarker}' is ` +
                      'not supported'));
    return;
  }
  if (!benchmarker.present) {
    callback(new Error(`Requested benchmarker '${options.benchmarker}' is ` +
                       'not installed'));
    return;
  }

  const benchmarker_start = process.hrtime();

  var child = benchmarker.instance.create(options);

  child.stderr.pipe(process.stderr);

  let stdout = '';
  child.stdout.on('data', (chunk) => stdout += chunk.toString());

  child.once('close', function(code) {
    const elapsed = process.hrtime(benchmarker_start);
    if (code) {
      var error_message = `${options.benchmarker} failed with ${code}.`;
      if (stdout !== '') {
        error_message += ` Output: ${stdout}`;
      }
      callback(new Error(error_message), code);
      return;
    }

    const result = benchmarker.instance.processResults(stdout);
    if (!result) {
      callback(new Error(`${options.benchmarker} produced strange output: ` +
                         stdout, code));
      return;
    }

    callback(null, code, options.benchmarker, result, elapsed);
  });

};

exports.default_http_benchmarker = default_http_benchmarker;
exports.supported_http_benchmarkers = supported_http_benchmarkers;
exports.installed_http_benchmarkers = installed_http_benchmarkers;

