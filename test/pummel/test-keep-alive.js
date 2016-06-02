'use strict';

// This test requires the program 'wrk'
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var http = require('http');
var url = require('url');

if (common.isWindows) {
  common.skip('no `wrk` on windows');
  return;
}

var body = 'hello world\n';
var server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Length': body.length,
    'Content-Type': 'text/plain'
  });
  res.write(body);
  res.end();
});

var keepAliveReqSec = 0;
var normalReqSec = 0;


function runAb(opts, callback) {
  var args = [
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

  //console.log(comm, args.join(' '));

  var child = spawn('wrk', args);
  child.stderr.pipe(process.stderr);
  child.stdout.setEncoding('utf8');

  var stdout;

  child.stdout.on('data', function(data) {
    stdout += data;
  });

  child.on('close', function(code, signal) {
    if (code) {
      console.error(code, signal);
      process.exit(code);
      return;
    }

    var matches = /Requests\/sec:\s*(\d+)\./mi.exec(stdout);
    var reqSec = parseInt(matches[1]);

    matches = /Keep-Alive requests:\s*(\d+)/mi.exec(stdout);
    var keepAliveRequests;
    if (matches) {
      keepAliveRequests = parseInt(matches[1]);
    } else {
      keepAliveRequests = 0;
    }

    callback(reqSec, keepAliveRequests);
  });
}

server.listen(common.PORT, function() {
  runAb({ keepalive: true }, function(reqSec) {
    keepAliveReqSec = reqSec;
    console.log('keep-alive:', keepAliveReqSec, 'req/sec');

    runAb({ keepalive: false }, function(reqSec) {
      normalReqSec = reqSec;
      console.log('normal:' + normalReqSec + ' req/sec');
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.equal(true, normalReqSec > 50);
  assert.equal(true, keepAliveReqSec > 50);
  assert.equal(true, normalReqSec < keepAliveReqSec);
});
