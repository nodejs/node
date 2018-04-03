'use strict';

const common = require('../common');

const assert = require('assert');
const http = require('http');

let time = Date.now();
let intervalWasInvoked = false;
const TIMEOUT = common.platformTimeout(200);

const server = http.createServer((req, res) => {
  server.close();

  res.writeHead(200);
  res.flushHeaders();

  req.setTimeout(TIMEOUT, () => {
    if (!intervalWasInvoked)
      return common.skip('interval was not invoked quickly enough for test');
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
  }, (res) => {
    const interval = setInterval(() => {
      intervalWasInvoked = true;
      // If machine is busy enough that the interval takes more than TIMEOUT ms
      // to be invoked, skip the test.
      const now = Date.now();
      if (now - time > TIMEOUT)
        return common.skip('interval is not invoked quickly enough for test');
      time = now;
      req.write('a');
    }, common.platformTimeout(25));
    setTimeout(() => {
      clearInterval(interval);
      req.end();
    }, TIMEOUT);
  });
  req.write('.');
}));
