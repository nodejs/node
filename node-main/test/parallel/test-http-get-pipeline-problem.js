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
// In previous versions of Node.js (e.g., 0.6.0), this sort of thing would halt
// after http.globalAgent.maxSockets number of files.
// See https://groups.google.com/forum/#!topic/nodejs-dev/V5fB69hFa9o
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const http = require('http');
const fs = require('fs');
const Countdown = require('../common/countdown');

http.globalAgent.maxSockets = 1;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const image = fixtures.readSync('/person.jpg');

console.log(`image.length = ${image.length}`);

const total = 10;
const responseCountdown = new Countdown(total, common.mustCall(() => {
  checkFiles();
  server.close();
}));

const server = http.Server(function(req, res) {
  setTimeout(function() {
    res.writeHead(200, {
      'content-type': 'image/jpeg',
      'connection': 'close',
      'content-length': image.length
    });
    res.end(image);
  }, 1);
});


server.listen(0, function() {
  for (let i = 0; i < total; i++) {
    (function() {
      const x = i;

      const opts = {
        port: server.address().port,
        headers: { connection: 'close' }
      };

      http.get(opts, function(res) {
        console.error(`recv ${x}`);
        const s = fs.createWriteStream(`${tmpdir.path}/${x}.jpg`);
        res.pipe(s);

        s.on('finish', function() {
          console.error(`done ${x}`);
          responseCountdown.dec();
        });
      }).on('error', function(e) {
        console.error('error! ', e.message);
        throw e;
      });
    })();
  }
});

function checkFiles() {
  // Should see 1.jpg, 2.jpg, ..., 100.jpg in tmpDir
  const files = fs.readdirSync(tmpdir.path);
  assert(total <= files.length);

  for (let i = 0; i < total; i++) {
    const fn = `${i}.jpg`;
    assert.ok(files.includes(fn), `couldn't find '${fn}'`);
    const stat = fs.statSync(`${tmpdir.path}/${fn}`);
    assert.strictEqual(
      image.length, stat.size,
      `size doesn't match on '${fn}'. Got ${stat.size} bytes`);
  }
}
