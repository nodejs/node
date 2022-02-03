'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

{
  const testResBody = 'other stuff!\n';

  // Checks the full 100-continue flow from client sending 'expect: 100-continue'
  // through server receiving it, sending back :status 100, writing the rest of
  // the request to finally the client receiving to.

  const server = http2.createServer();

  let sentResponse = false;

  server.on('request', common.mustCall((req, res) => {
    res.end(testResBody);
    sentResponse = true;
  }));

  server.listen(0);

  server.on('listening', common.mustCall(() => {
    let body = '';

    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request({
      ':method': 'POST',
      'expect': '100-continue'
    });

    let gotContinue = false;
    req.on('continue', common.mustCall(() => {
      gotContinue = true;
    }));

    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(gotContinue, true);
      assert.strictEqual(sentResponse, true);
      assert.strictEqual(headers[':status'], 200);
      req.end();
    }));

    req.setEncoding('utf8');
    req.on('data', common.mustCall((chunk) => { body += chunk; }));
    req.on('end', common.mustCall(() => {
      assert.strictEqual(body, testResBody);
      client.close();
      server.close();
    }));
  }));
}

{
  // Checks the full 100-continue flow from client sending 'expect: 100-continue'
  // through server receiving it and ending the request.

  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    res.end();
  }));

  server.listen(0);

  server.on('listening', common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request({
      ':path': '/',
      'expect': '100-continue'
    });

    let gotContinue = false;
    req.on('continue', common.mustCall(() => {
      gotContinue = true;
    }));

    let gotResponse = false;
    req.on('response', common.mustCall(() => {
      gotResponse = true;
    }));

    req.setEncoding('utf8');
    req.on('end', common.mustCall(() => {
      assert.strictEqual(gotContinue, true);
      assert.strictEqual(gotResponse, true);
      client.close();
      server.close();
    }));
  }));
}
