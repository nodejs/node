'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => {
  res.removeHeader('header1', 1);
  res.write('abc');
  assert.throws(
    () => res.removeHeader('header2', 2),
    {
      code: 'ERR_HTTP_HEADERS_SENT',
      name: 'Error',
      message: 'Cannot remove headers after they are sent to the client'
    }
  );
  res.end();
});

server.listen(0, () => {
  http.get({ port: server.address().port }, () => {
    server.close();
  });
});
