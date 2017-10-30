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
// see https://github.com/joyent/node/issues/3257

const common = require('../common');
const http = require('http');

const server = http.createServer(function(req, res) {
  req.resume();
  req.once('end', function() {
    res.writeHead(200);
    res.end();
    server.close();
  });
});

common.refreshTmpDir();

server.listen(common.PIPE, function() {
  const req = http.request({
    socketPath: common.PIPE,
    headers: { 'Content-Length': '1' },
    method: 'POST',
    path: '/'
  });

  req.write('.');

  sched(function() { req.end(); }, 5);
});

// schedule a callback after `ticks` event loop ticks
function sched(cb, ticks) {
  function fn() {
    if (--ticks)
      setImmediate(fn);
    else
      cb();
  }
  setImmediate(fn);
}
