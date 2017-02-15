'use strict';

// This test requires the program 'wrk'
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const http = require('http');
const url = require('url');

if (common.isWindows) {
  common.skip('no `wrk` on windows');
  return;
}

const body = 'hello world\n';
const server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Length': body.length,
    'Content-Type': 'text/plain'
  });
  res.write(body);
  res.end();
});

let keepAliveReqSec = 0;
let normalReqSec = 0;


function runAb(opts, callback) {
  const args = [
    '-c', opts.concurrent || 100,
    '-t', opts.threads || 2,
    '-d', opts.duration || '10s',
  ];

  if (!opts.keepalive) {
    args.push('-H');
    args.push('Connection: close');
  }

  args.push(url.format({ hostname: '127.0.0.1',
                         port: common.PORT, protocol: 'http'}));

  const child = spawn('wrk', args);
  child.stderr.pipe(process.stderr);
  child.stdout.setEncoding('utf8');

  let stdout;

  child.stdout.on('data', function(data) {
    stdout += data;
  });

  child.on('close', function(code, signal) {
    if (code) {
      console.error(code, signal);
      process.exit(code);
      return;
    }

    let matches = /Requests\/sec:\s*(\d+)\./mi.exec(stdout);
    const reqSec = parseInt(matches[1]);

    matches = /Keep-Alive requests:\s*(\d+)/mi.exec(stdout);
    let keepAliveRequests;
    if (matches) {
      keepAliveRequests = parseInt(matches[1]);
    } else {
      keepAliveRequests = 0;
    }

    callback(reqSec, keepAliveRequests);
  });
}

server.listen(common.PORT, () => {
  runAb({ keepalive: true }, (reqSec) => {
    keepAliveReqSec = reqSec;

    runAb({ keepalive: false }, function(reqSec) {
      normalReqSec = reqSec;
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.strictEqual(true, normalReqSec > 50);
  assert.strictEqual(true, keepAliveReqSec > 50);
  assert.strictEqual(true, normalReqSec < keepAliveReqSec);
});
