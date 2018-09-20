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
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Type': 'text/plain',
    'Connection': 'close'
  });
  res.write('hello ');
  res.write('world\n');
  res.end();
});

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

server.listen(common.PIPE, common.mustCall(function() {

  const options = {
    socketPath: common.PIPE,
    path: '/'
  };

  const req = http.get(options, common.mustCall(function(res) {
    assert.strictEqual(res.statusCode, 200);
    assert.strictEqual(res.headers['content-type'], 'text/plain');

    res.body = '';
    res.setEncoding('utf8');

    res.on('data', function(chunk) {
      res.body += chunk;
    });

    res.on('end', common.mustCall(function() {
      assert.strictEqual(res.body, 'hello world\n');
      server.close(common.mustCall(function(error) {
        assert.strictEqual(error, undefined);
        server.close(common.expectsError({
          code: 'ERR_SERVER_NOT_RUNNING',
          message: 'Server is not running.',
          type: Error
        }));
      }));
    }));
  }));

  req.on('error', function(e) {
    assert.fail(e.stack);
  });

  req.end();

}));
