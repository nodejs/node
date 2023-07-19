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
const assert = require('assert');
const net = require('net');
const debuglog = require('util').debuglog('test');

let chars_recved = 0;
let npauses = 0;
let totalLength = 0;

const server = net.createServer((connection) => {
  const body = 'C'.repeat(1024);
  let n = 1;
  debuglog('starting write loop');
  while (connection.write(body)) {
    n++;
  }
  debuglog('ended write loop');
  // Now that we're throttled, do some more writes to make sure the data isn't
  // lost.
  connection.write(body);
  connection.write(body);
  n += 2;
  totalLength = n * body.length;
  assert.ok(connection.bufferSize >= 0, `bufferSize: ${connection.bufferSize}`);
  assert.ok(
    connection.writableLength <= totalLength,
    `writableLength: ${connection.writableLength}, totalLength: ${totalLength}`,
  );
  connection.end();
});

server.listen(0, () => {
  const port = server.address().port;
  debuglog(`server started on port ${port}`);
  let paused = false;
  const client = net.createConnection(port);
  client.setEncoding('ascii');
  client.on('data', (d) => {
    chars_recved += d.length;
    debuglog(`got ${chars_recved}`);
    if (!paused) {
      client.pause();
      npauses += 1;
      paused = true;
      debuglog('pause');
      const x = chars_recved;
      setTimeout(() => {
        assert.strictEqual(chars_recved, x);
        client.resume();
        debuglog('resume');
        paused = false;
      }, 100);
    }
  });

  client.on('end', () => {
    server.close();
    client.end();
  });
});


process.on('exit', () => {
  assert.strictEqual(chars_recved, totalLength);
  assert.ok(npauses > 1, `${npauses} > 1`);
});
