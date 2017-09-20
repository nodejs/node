'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => {
  assert.doesNotThrow(() => {
    res.setHeader('header1', 1);
  });
  res.write('abc');
  common.expectsError(
    () => res.setHeader('header2', 2),
    {
      code: 'ERR_HTTP_HEADERS_SENT',
      type: Error,
      message: 'Cannot set headers after they are sent to the client'
    }
  );
  res.end();
});

server.listen(0, () => {
  http.get({ port: server.address().port }, () => {
    server.close();
  });
});
