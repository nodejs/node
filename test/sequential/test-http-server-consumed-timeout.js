'use strict';

const common = require('../common');

const assert = require('assert');
const http = require('http');

let intervalWasInvoked = false;
const TIMEOUT = common.platformTimeout(50);

runTest(TIMEOUT);

function runTest(timeoutDuration) {
  const server = http.createServer((req, res) => {
    server.close();

    res.writeHead(200);
    res.flushHeaders();

    req.setTimeout(timeoutDuration, () => {
      if (!intervalWasInvoked) {
        // Interval wasn't invoked, probably because the machine is busy with
        // other things. Try again with a longer timeout.
        console.error(`Retrying with timeout of ${timeoutDuration * 2}.`);
        return setImmediate(() => { runTest(timeoutDuration * 2); });
      }
      assert.fail('Request timeout should not fire');
    });

    req.resume();
    req.once('end', () => {
      res.end();
    });
  });

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      method: 'POST'
    }, () => {
      const interval = setInterval(() => {
        intervalWasInvoked = true;
        req.write('a');
      }, 25);
      setTimeout(() => {
        clearInterval(interval);
        req.end();
      }, timeoutDuration);
    });
    req.write('.');
  }));
}
