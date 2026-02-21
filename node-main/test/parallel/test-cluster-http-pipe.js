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
if (common.isWindows) {
  common.skip(
    'It is not possible to send pipe handles over the IPC pipe on Windows');
}

const assert = require('assert');
const cluster = require('cluster');
const http = require('http');

if (cluster.isPrimary) {
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const worker = cluster.fork();
  worker.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'DONE');
  }));
  worker.on('exit', common.mustCall());
  return;
}

http.createServer(common.mustCall((req, res) => {
  assert.strictEqual(req.connection.remoteAddress, undefined);
  assert.strictEqual(req.connection.localAddress, undefined);

  res.writeHead(200);
  res.end('OK');
})).listen(common.PIPE, common.mustCall(() => {
  http.get({ socketPath: common.PIPE, path: '/' }, common.mustCall((res) => {
    res.resume();
    res.on('end', common.mustSucceed(() => {
      process.send('DONE');
      process.exit();
    }));
  }));
}));
