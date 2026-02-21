'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

// This test case ensures that calling of res.end after sending
// 204, 205 and 304 HTTP statuses will not cause an error
// See issue: https://github.com/nodejs/node/issues/21740

const {
  HTTP_STATUS_NO_CONTENT,
  HTTP_STATUS_RESET_CONTENT,
  HTTP_STATUS_NOT_MODIFIED
} = h2.constants;

const statusWithoutBody = [
  HTTP_STATUS_NO_CONTENT,
  HTTP_STATUS_RESET_CONTENT,
  HTTP_STATUS_NOT_MODIFIED,
];
const STATUS_CODES_COUNT = statusWithoutBody.length;

const server = h2.createServer(common.mustCall(function(req, res) {
  res.writeHead(statusWithoutBody.pop());
  res.end();
}, STATUS_CODES_COUNT));

server.listen(0, common.mustCall(function() {
  const url = `http://localhost:${server.address().port}`;
  const client = h2.connect(url, common.mustCall(() => {
    let responseCount = 0;
    const closeAfterResponse = () => {
      if (STATUS_CODES_COUNT === ++responseCount) {
        client.destroy();
        server.close();
      }
    };

    for (let i = 0; i < STATUS_CODES_COUNT; i++) {
      const request = client.request();
      request.on('response', common.mustCall(closeAfterResponse));
    }

  }));
}));
