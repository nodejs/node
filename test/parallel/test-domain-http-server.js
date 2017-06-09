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

'use strict';
require('../common');
const domain = require('domain');
const http = require('http');
const assert = require('assert');

const objects = { foo: 'bar', baz: {}, num: 42, arr: [1, 2, 3] };
objects.baz.asdf = objects;

let serverCaught = 0;
let clientCaught = 0;

const server = http.createServer(function(req, res) {
  const dom = domain.create();
  req.resume();
  dom.add(req);
  dom.add(res);

  dom.on('error', function(er) {
    serverCaught++;
    console.log('horray! got a server error', er);
    // try to send a 500.  If that fails, oh well.
    res.writeHead(500, {'content-type': 'text/plain'});
    res.end(er.stack || er.message || 'Unknown error');
  });

  dom.run(function() {
    // Now, an action that has the potential to fail!
    // if you request 'baz', then it'll throw a JSON circular ref error.
    const data = JSON.stringify(objects[req.url.replace(/[^a-z]/g, '')]);

    // this line will throw if you pick an unknown key
    assert.notStrictEqual(data, undefined, 'Data should not be undefined');

    res.writeHead(200);
    res.end(data);
  });
});

server.listen(0, next);

function next() {
  const port = this.address().port;
  console.log('listening on localhost:%d', port);

  let requests = 0;
  let responses = 0;

  makeReq('/');
  makeReq('/foo');
  makeReq('/arr');
  makeReq('/baz');
  makeReq('/num');

  function makeReq(p) {
    requests++;

    const dom = domain.create();
    dom.on('error', function(er) {
      clientCaught++;
      console.log('client error', er);
      req.socket.destroy();
    });

    const req = http.get({ host: 'localhost', port: port, path: p });
    dom.add(req);
    req.on('response', function(res) {
      responses++;
      console.error('requests=%d responses=%d', requests, responses);
      if (responses === requests) {
        console.error('done, closing server');
        // no more coming.
        server.close();
      }

      dom.add(res);
      let d = '';
      res.on('data', function(c) {
        d += c;
      });
      res.on('end', function() {
        console.error('trying to parse json', d);
        d = JSON.parse(d);
        console.log('json!', d);
      });
    });
  }
}

process.on('exit', function() {
  assert.strictEqual(serverCaught, 2);
  assert.strictEqual(clientCaught, 2);
  console.log('ok');
});
