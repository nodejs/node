'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

// first 204 or 304 works, subsequent anything fails
const codes = [204, 200];

// Methods don't really matter, but we put in something realistic.
const methods = ['DELETE', 'DELETE'];

const server = http.createServer(common.mustCall((req, res) => {
  const code = codes.shift();
  assert.strictEqual(typeof code, 'number');
  assert.ok(code > 0);
  res.writeHead(code, {});
  res.end();
}, codes.length));

function nextRequest() {
  const method = methods.shift();

  const request = http.request({
    port: server.address().port,
    method: method,
    path: '/'
  }, common.mustCall((response) => {
    response.on('end', common.mustCall(() => {
      if (methods.length === 0) {
        server.close();
      } else {
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
