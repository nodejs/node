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

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('Hello World\n');
}).listen(common.PORT);

var agent = new http.Agent({maxSockets: 1});

agent.on('free', function(socket, host, port) {
  console.log('freeing socket. destroyed? ', socket.destroyed);
});

var requestOptions = {
  agent: agent,
  host: 'localhost',
  port: common.PORT,
  path: '/'
};

var request1 = http.get(requestOptions, function(response) {
  // assert request2 is queued in the agent
  var key = agent.getName(requestOptions);
  assert(agent.requests[key].length === 1);
  console.log('got response1');
  request1.socket.on('close', function() {
    console.log('request1 socket closed');
  });
  response.pipe(process.stdout);
  response.on('end', function() {
    console.log('response1 done');
    /////////////////////////////////
    //
    // THE IMPORTANT PART
    //
    // It is possible for the socket to get destroyed and other work
    // to run before the 'close' event fires because it happens on
    // nextTick. This example is contrived because it destroys the
    // socket manually at just the right time, but at Voxer we have
    // seen cases where the socket is destroyed by non-user code
    // then handed out again by an agent *before* the 'close' event
    // is triggered.
    request1.socket.destroy();

    response.once('close', function() {
      // assert request2 was removed from the queue
      assert(!agent.requests[key]);
      console.log("waiting for request2.onSocket's nextTick");
      process.nextTick(function() {
        // assert that the same socket was not assigned to request2,
        // since it was destroyed.
        assert(request1.socket !== request2.socket);
        assert(!request2.socket.destroyed, 'the socket is destroyed');
      });
    });
  });
});

var request2 = http.get(requestOptions, function(response) {
  assert(!request2.socket.destroyed);
  assert(request1.socket.destroyed);
  // assert not reusing the same socket, since it was destroyed.
  assert(request1.socket !== request2.socket);
  console.log('got response2');
  var gotClose = false;
  var gotResponseEnd = false;
  request2.socket.on('close', function() {
    console.log('request2 socket closed');
    gotClose = true;
    done();
  });
  response.pipe(process.stdout);
  response.on('end', function() {
    console.log('response2 done');
    gotResponseEnd = true;
    done();
  });

  function done() {
    if (gotResponseEnd && gotClose)
      server.close();
  }
});
