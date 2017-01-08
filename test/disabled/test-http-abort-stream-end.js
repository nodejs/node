'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');

const maxSize = 1024;
let size = 0;

const s = http.createServer(function(req, res) {
  this.close();

  res.writeHead(200, {'Content-Type': 'text/plain'});
  for (let i = 0; i < maxSize; i++) {
    res.write('x' + i);
  }
  res.end();
});

let aborted = false;
s.listen(common.PORT, () => {
  const req = http.get(`http://localhost:${common.PORT}`, (res) => {
    res.on('data', (chunk) => {
      size += chunk.length;
      assert(!aborted, 'got data after abort');
      if (size > maxSize) {
        aborted = true;
        req.abort();
        size = maxSize;
      }
    });
  });

  req.end();
});

process.on('exit', () => {
  assert(aborted);
  assert.equal(size, maxSize);
  console.log('ok');
});
