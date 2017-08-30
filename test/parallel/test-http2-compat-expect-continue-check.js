// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const testResBody = 'other stuff!\n';

// Checks the full 100-continue flow from client sending 'expect: 100-continue'
// through server receiving it, triggering 'checkContinue' custom handler,
// writing the rest of the request to finally the client receiving to.

function handler(req, res) {
  console.error('Server sent full response');

  res.writeHead(200, {
    'content-type': 'text/plain',
    'abcd': '1'
  });
  res.end(testResBody);
}

const server = http2.createServer(
  common.mustNotCall('Full request received before 100 Continue')
);

server.on('checkContinue', common.mustCall((req, res) => {
  console.error('Server received Expect: 100-continue');

  res.writeContinue();

  // timeout so that we allow the client to receive continue first
  setTimeout(
    common.mustCall(() => handler(req, res)),
    common.platformTimeout(100)
  );
}));

server.listen(0);

server.on('listening', common.mustCall(() => {
  let body = '';

  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request({
    ':method': 'POST',
    ':path': '/world',
    expect: '100-continue'
  });
  console.error('Client sent request');

  let gotContinue = false;
  req.on('continue', common.mustCall(() => {
    console.error('Client received 100-continue');
    gotContinue = true;
  }));

  req.on('response', common.mustCall((headers) => {
    console.error('Client received response headers');

    assert.strictEqual(gotContinue, true);
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers['abcd'], '1');
  }));

  req.setEncoding('utf-8');
  req.on('data', common.mustCall((chunk) => { body += chunk; }));

  req.on('end', common.mustCall(() => {
    console.error('Client received full response');

    assert.strictEqual(body, testResBody);

    client.destroy();
    server.close();
  }));
}));
