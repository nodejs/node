'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var requests_sent = 0;
var requests_done = 0;
var options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
};

//http.globalAgent.maxSockets = 15;

var server = http.createServer(function(req, res) {
  const m = /\/(.*)/.exec(req.url);
  const reqid = parseInt(m[1], 10);
  if (reqid % 2) {
    // do not reply the request
  } else {
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write(reqid.toString());
    res.end();
  }
});

server.listen(0, options.host, function() {
  options.port = this.address().port;
  var req;

  for (requests_sent = 0; requests_sent < 30; requests_sent += 1) {
    options.path = '/' + requests_sent;
    req = http.request(options);
    req.id = requests_sent;
    req.on('response', function(res) {
      res.on('data', function(data) {
        console.log('res#' + this.req.id + ' data:' + data);
      });
      res.on('end', function(data) {
        console.log('res#' + this.req.id + ' end');
        requests_done += 1;
      });
    });
    req.on('close', function() {
      console.log('req#' + this.id + ' close');
    });
    req.on('error', function() {
      console.log('req#' + this.id + ' error');
      this.destroy();
    });
    req.setTimeout(50, function() {
      console.log('req#' + this.id + ' timeout');
      this.abort();
      requests_done += 1;
    });
    req.end();
  }

  setTimeout(function maybeDone() {
    if (requests_done >= requests_sent) {
      setTimeout(function() {
        server.close();
      }, 100);
    } else {
      setTimeout(maybeDone, 100);
    }
  }, 100);
});

process.on('exit', function() {
  console.error('done=%j sent=%j', requests_done, requests_sent);
  assert.strictEqual(requests_done, requests_sent,
                     'timeout on http request called too much');
});
