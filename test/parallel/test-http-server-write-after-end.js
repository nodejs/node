'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

// Fix for https://github.com/nodejs/node/issues/14368

const server = http.createServer(handle);

function handle(req, res) {
  res.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'write after end');
    server.close();
  }));

  res.write('hello');
  res.end();

  setImmediate(common.mustCall(() => {
    res.write('world');
  }));
}

server.listen(0, common.mustCall(() => {
  http.get(`http://localhost:${server.address().port}`);
}));
