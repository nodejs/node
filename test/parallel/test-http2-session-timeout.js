'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const hrtime = process.hrtime.bigint;
const NS_PER_MS = 1_000_000n;

let requests = 0;
const mustNotCall = () => {
  assert.fail(`Timeout after ${requests} request(s)`);
};

const server = http2.createServer();
// Disable server timeout until first request. We will set the timeout based on
// how long the first request takes.
server.timeout = 0n;

server.on('request', (req, res) => res.end());
server.on('timeout', mustNotCall);

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  const url = `http://localhost:${port}`;
  const client = http2.connect(url);
  let startTime = hrtime();
  makeReq();

  function makeReq() {
    const request = client.request({
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`,
    });
    request.resume();
    request.end();

    requests += 1;

    request.on('end', () => {
      const diff = hrtime() - startTime;
      const milliseconds = diff / NS_PER_MS;
      if (server.timeout === 0n) {
        // Set the timeout now. First connection will take significantly longer
        // than subsequent connections, so using the duration of the first
        // connection as the timeout should be robust. Double it anyway for good
        // measure.
        server.timeout = milliseconds * 2n;
        startTime = hrtime();
        makeReq();
      } else if (milliseconds < server.timeout * 2n) {
        makeReq();
      } else {
        server.removeListener('timeout', mustNotCall);
        server.close();
        client.close();
      }
    });
  }
}));
