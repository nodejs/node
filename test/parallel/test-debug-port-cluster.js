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
const spawn = require('child_process').spawn;

const PORT_MIN = common.PORT + 1;  // The fixture uses common.PORT.
const PORT_MAX = PORT_MIN + 2;

const args = [
  '--debug=' + PORT_MIN,
  common.fixturesDir + '/clustered-server/app.js'
];

const child = spawn(process.execPath, args);
child.stderr.setEncoding('utf8');

const checkMessages = common.mustCall(() => {
  for (let port = PORT_MIN; port <= PORT_MAX; port += 1) {
    assert(stderr.includes(`Debugger listening on 127.0.0.1:${port}`));
  }
});

let stderr = '';
child.stderr.on('data', (data) => {
  process.stderr.write(`[DATA] ${data}`);
  stderr += data;
  if (child.killed !== true && stderr.includes('all workers are running')) {
    child.kill();
    checkMessages();
  }
});
