'use strict';

const child_process = require('child_process');

function AutocannonBenchmarker() {
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
    if (!result)
      return undefined;
    else
      return result;
  };
}

exports.HTTPBenchmarkers = {
  autocannon: new AutocannonBenchmarker(),
  wrk: new WrkBenchmarker()
};
