'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

const reqstr = 'POST / HTTP/1.1\r\n' +
               'Content-Length: 1\r\n' +
               'Transfer-Encoding: chunked\r\n\r\n';

const server = http.createServer((req, res) => {
  assert.fail(null, null, 'callback should not be invoked');
});
server.on('clientError', common.mustCall((err) => {
  assert(/^Parse Error/.test(err.message));
  assert.equal(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
  server.close();
}));
server.listen(common.PORT, () => {
  const client = net.connect({port: common.PORT}, () => {
    client.write(reqstr);
    client.end();
  });
  client.on('data', (data) => {
    // Should not get to this point because the server should simply
    // close the connection without returning any data.
    assert.fail(null, null, 'no data should be returned by the server');
  });
  client.on('end', common.mustCall(() => {}));
});
