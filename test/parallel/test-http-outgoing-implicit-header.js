'use strict';

require('../common');

const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => {
  res._hasBody = false;
  res.end();
});

server.listen(3000, () => {
  console.log('Server is listening on port 3000');

  http.get('http://localhost:3000', (res) => {
    assert.throws(() => {
      res.write('test');
    }, Error);
    server.close();
  });
});
