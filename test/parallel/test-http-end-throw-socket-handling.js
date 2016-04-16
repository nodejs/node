'use strict';
const common = require('../common');
const assert = require('assert');

// Make sure that throwing in 'end' handler doesn't lock
// up the socket forever.
//
// This is NOT a good way to handle errors in general, but all
// the same, we should not be so brittle and easily broken.

const http = require('http');

let n = 0;
const server = http.createServer((req, res) => {
  if (++n === 10) server.close();
  res.end('ok');
});

server.listen(common.PORT, common.mustCall(() => {
  for (let i = 0; i < 10; i++) {
    const options = { port: common.PORT };
    const req = http.request(options, (res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        throw new Error('gleep glorp');
      }));
    });
    req.end();
  }
}));

let errors = 0;
process.on('uncaughtException', () => {
  errors++;
});

process.on('exit', () => {
  assert.equal(errors, 10);
});
