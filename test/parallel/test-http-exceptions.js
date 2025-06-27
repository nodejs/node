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
const Countdown = require('../common/countdown');
const http = require('http');
const NUMBER_OF_EXCEPTIONS = 4;
const countdown = new Countdown(NUMBER_OF_EXCEPTIONS, () => {
  process.exit(0);
});

const server = http.createServer(function(req, res) {
  intentionally_not_defined(); // eslint-disable-line no-undef
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.write('Thank you, come again.');
  res.end();
});

function onUncaughtException(err) {
  console.log(`Caught an exception: ${err}`);
  if (err.name === 'AssertionError') throw err;
  countdown.dec();
}

process.on('uncaughtException', onUncaughtException);

server.listen(0, function() {
  for (let i = 0; i < NUMBER_OF_EXCEPTIONS; i += 1) {
    http.get({ port: this.address().port, path: `/busy/${i}` });
  }
});
