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
var http = require('http');

var request_number = 0;
var requests_sent = 0;
var requests_done = 0;
var options = {
  method: 'GET',
  port: common.PORT,
  host: '127.0.0.1',
};

//http.globalAgent.maxSockets = 15;

var server = http.createServer(function(req, res) {
  var m = /\/(.*)/.exec(req.url),
      reqid = parseInt(m[1], 10);
  if ( reqid % 2 ) {
    // do not reply the request
  } else {
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write(reqid.toString());
    res.end();
  }
  request_number+=1;
});

server.listen(options.port, options.host, function() {
  var req;

  for (requests_sent = 0; requests_sent < 30; requests_sent+=1) {
    options.path = '/' + requests_sent;
    req = http.request(options);
    req.id = requests_sent;
    req.on('response', function(res) {
      res.on('data', function(data) {
        console.log('res#'+this.req.id+' data:'+data);
      });
      res.on('end', function(data) {
        console.log('res#'+this.req.id+' end');
        requests_done += 1;
      });
    });
    req.on('close', function() {
      console.log('req#'+this.id+' close');
    });
    req.on('error', function() {
      console.log('req#'+this.id+' error');
      this.destroy();
    });
    req.setTimeout(50, function () {
      var req = this;
      console.log('req#'+this.id + ' timeout');
      req.abort();
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
  assert.ok(requests_done == requests_sent, 'timeout on http request called too much');
});
