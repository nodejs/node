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
// USE OR OTHER DEALINGS IN THE SOFTWARE.




var common = require('../common');
var assert = require('assert');
var util = require('util'),
    fs = require('fs'),
    http = require('http'),
    url = require('url');

var chunk = '01234567890123456789';

// Produce a very large response.
var chargen = http.createServer(function(req, res) {
  var len = parseInt(req.headers['x-len'], 10);
  assert.ok(len > 0);
  res.writeHead(200, {'transfer-encoding': 'chunked'});
  for (var i = 0; i < len; i++) {
    if (i % 1000 == 0) common.print(',');
    res.write(chunk);
  }
  res.end();
});
chargen.listen(9000, ready);

// Proxy to the chargen server.
var proxy = http.createServer(function(req, res) {
  var len = parseInt(req.headers['x-len'], 10);
  assert.ok(len > 0);

  var sent = 0;


  function onError(e) {
    console.log('proxy client error. sent ' + sent);
    throw e;
  }

  var proxy_req = http.request({
    host: 'localhost',
    port: 9000,
    method: req.method,
    path: req.url,
    headers: req.headers
  }, function(proxy_res) {
    res.writeHead(proxy_res.statusCode, proxy_res.headers);

    var count = 0;

    proxy_res.on('data', function(d) {
      if (count++ % 1000 == 0) common.print('.');
      res.write(d);
      sent += d.length;
      assert.ok(sent <= (len * chunk.length));
    });

    proxy_res.on('end', function() {
      res.end();
    });

  });
  proxy_req.on('error', onError);
  proxy_req.end();
});
proxy.listen(9001, ready);

var done = false;

function call_chargen(list) {
  if (list.length > 0) {
    var len = list.shift();

    common.debug('calling chargen for ' + len + ' chunks.');

    var recved = 0;

    var req = http.request({
      port: 9001,
      host: 'localhost',
      path: '/',
      headers: {'x-len': len}
    }, function(res) {

      res.on('data', function(d) {
        recved += d.length;
        assert.ok(recved <= (len * chunk.length));
      });

      res.on('end', function() {
        assert.ok(recved <= (len * chunk.length));
        common.debug('end for ' + len + ' chunks.');
        call_chargen(list);
      });

    });
    req.end();

  } else {
    console.log('End of list. closing servers');
    proxy.close();
    chargen.close();
    done = true;
  }
}

var serversRunning = 0;
function ready() {
  if (++serversRunning < 2) return;
  call_chargen([100, 1000, 10000, 100000, 1000000]);
}

process.on('exit', function() {
  assert.ok(done);
});
