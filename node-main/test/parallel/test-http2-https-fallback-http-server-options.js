'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const url = require('url');
const tls = require('tls');
const http2 = require('http2');
const https = require('https');
const http = require('http');

const key = fixtures.readKey('agent8-key.pem');
const cert = fixtures.readKey('agent8-cert.pem');
const ca = fixtures.readKey('fake-startcom-root-cert.pem');

function onRequest(request, response) {
  const { socket: { alpnProtocol } } = request.httpVersion === '2.0' ?
    request.stream.session : request;
  response.status(200);
  response.end(JSON.stringify({
    alpnProtocol,
    httpVersion: request.httpVersion,
    userAgent: request.getUserAgent()
  }));
}

class MyIncomingMessage extends http.IncomingMessage {
  getUserAgent() {
    return this.headers['user-agent'] || 'unknown';
  }
}

class MyServerResponse extends http.ServerResponse {
  status(code) {
    return this.writeHead(code, { 'Content-Type': 'application/json' });
  }
}

// HTTP/2 & HTTP/1.1 server
{
  const server = http2.createSecureServer(
    {
      cert,
      key, allowHTTP1: true,
      Http1IncomingMessage: MyIncomingMessage,
      Http1ServerResponse: MyServerResponse
    },
    common.mustCall(onRequest, 1)
  );

  server.listen(0);

  server.on('listening', common.mustCall(() => {
    const { port } = server.address();
    const origin = `https://localhost:${port}`;

    // HTTP/1.1 client
    https.get(
      Object.assign(url.parse(origin), {
        secureContext: tls.createSecureContext({ ca }),
        headers: { 'User-Agent': 'node-test' }
      }),
      common.mustCall((response) => {
        assert.strictEqual(response.statusCode, 200);
        assert.strictEqual(response.statusMessage, 'OK');
        assert.strictEqual(
          response.headers['content-type'],
          'application/json'
        );

        response.setEncoding('utf8');
        let raw = '';
        response.on('data', (chunk) => { raw += chunk; });
        response.on('end', common.mustCall(() => {
          const { alpnProtocol, httpVersion, userAgent } = JSON.parse(raw);
          assert.strictEqual(alpnProtocol, false);
          assert.strictEqual(httpVersion, '1.1');
          assert.strictEqual(userAgent, 'node-test');

          server.close();
        }));
      })
    );
  }));
}
