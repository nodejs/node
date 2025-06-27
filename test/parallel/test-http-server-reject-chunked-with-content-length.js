'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

const reqstr = 'POST / HTTP/1.1\r\n' +
               'Content-Length: 1\r\n' +
               'Transfer-Encoding: chunked\r\n\r\n';

const server = http.createServer(common.mustNotCall());
server.on('clientError', common.mustCall((err) => {
  assert.match(err.message, /^Parse Error/);
  assert.strictEqual(err.code, 'HPE_INVALID_TRANSFER_ENCODING');
  server.close();
}));
server.listen(0, () => {
  const client = net.connect({ port: server.address().port }, () => {
    client.write(reqstr);
    client.end();
  });
  client.on('data', (data) => {
    // Should not get to this point because the server should simply
    // close the connection without returning any data.
    assert.fail('no data should be returned by the server');
  });
  client.on('end', common.mustCall());
});
