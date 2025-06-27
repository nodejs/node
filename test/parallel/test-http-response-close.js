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
const http = require('http');
const assert = require('assert');

{
  const server = http.createServer(
    common.mustCall((req, res) => {
      res.writeHead(200);
      res.write('a');
    })
  );
  server.listen(
    0,
    common.mustCall(() => {
      http.get(
        { port: server.address().port },
        common.mustCall((res) => {
          res.on('data', common.mustCall(() => {
            res.destroy();
          }));
          assert.strictEqual(res.destroyed, false);
          res.on('close', common.mustCall(() => {
            assert.strictEqual(res.destroyed, true);
            server.close();
          }));
        })
      );
    })
  );
}

{
  const server = http.createServer(
    common.mustCall((req, res) => {
      res.writeHead(200);
      res.end('a');
    })
  );
  server.listen(
    0,
    common.mustCall(() => {
      http.get(
        { port: server.address().port },
        common.mustCall((res) => {
          assert.strictEqual(res.destroyed, false);
          res.on('end', common.mustCall(() => {
            assert.strictEqual(res.destroyed, false);
          }));
          res.on('close', common.mustCall(() => {
            assert.strictEqual(res.destroyed, true);
            server.close();
          }));
          res.resume();
        })
      );
    })
  );
}

{
  const server = http.createServer(
    common.mustCall((req, res) => {
      res.on('close', common.mustCall());
      res.destroy();
    })
  );

  server.listen(
    0,
    common.mustCall(() => {
      http.get(
        { port: server.address().port },
        common.mustNotCall()
      )
      .on('error', common.mustCall(() => {
        server.close();
      }));
    })
  );
}
