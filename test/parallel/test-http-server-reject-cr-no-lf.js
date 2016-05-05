'use strict';

const common = require('../common');
const net = require('net');
const http = require('http');
const assert = require('assert');

const str = 'GET / HTTP/1.1\r\n' +
            'Dummy: Header\r' +
            'Content-Length: 1\r\n' +
            '\r\n';


const server = http.createServer((req, res) => {
  assert.fail(null, null, 'this should not be called');
});
server.on('clientError', common.mustCall((err) => {
  assert(/^Parse Error/.test(err.message));
  assert.equal(err.code, 'HPE_LF_EXPECTED');
  server.close();
}));
server.listen(common.PORT, () => {
  const client = net.connect({port: common.PORT}, () => {
    client.on('data', (chunk) => {
      assert.fail(null, null, 'this should not be called');
    });
    client.on('end', common.mustCall(() => {
      server.close();
    }));
    client.write(str);
    client.end();
  });
});
