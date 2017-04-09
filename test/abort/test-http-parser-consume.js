'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('hello');
  }));

  server.listen(0, common.mustCall((s) => {
    const rr = http.get(
      { port: server.address().port },
      common.mustCall((d) => {
        // This bad input (0) should abort the parser and the process
        rr.parser.consume(0);
        server.close();
      }));
  }));
} else {
  const child = spawn(process.execPath, [__filename, 'child'],
                      { stdio: 'inherit' });
  child.on('exit', common.mustCall((code, signal) => {
    assert(common.nodeProcessAborted(code, signal),
           'process should have aborted, but did not');
  }));
}
