'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');
const Countdown = require('../common/countdown');

// first 204 or 304 works, subsequent anything fails
const codes = [204, 200];

const countdown = new Countdown(codes.length, () => server.close());

const server = http.createServer(common.mustCall((req, res) => {
  const code = codes.shift();
  assert.strictEqual(typeof code, 'number');
  assert.ok(code > 0);
  res.writeHead(code, {});
  res.end();
}, codes.length));

function nextRequest() {

  const request = http.get({
    port: server.address().port,
    path: '/'
  }, common.mustCall((response) => {
    response.on('end', common.mustCall(() => {
      if (countdown.dec()) {
        // throws error:
        nextRequest();
        // works just fine:
        //process.nextTick(nextRequest);
      }
    }));
    response.resume();
  }));
  request.end();
}

server.listen(0, nextRequest);
