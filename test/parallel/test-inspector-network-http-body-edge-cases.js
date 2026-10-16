// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { once } = require('node:events');
const http = require('node:http');
const { Session } = require('node:inspector/promises');

async function main() {
  const session = new Session();
  session.connect();
  await session.post('Network.enable');

  // Empty writes must not produce body diagnostics. Only the two non-empty
  // chunks in the final request should reach subscribers.
  const onChunk = common.mustCall(({ chunk }) => {
    assert.ok(chunk.byteLength > 0);
  }, 2);
  const onBody = common.mustCall();
  dc.subscribe('http.client.request.bodyChunkSent', onChunk);
  dc.subscribe('http.client.request.bodySent', onBody);

  const payload = Buffer.from('before🙂after');
  const server = http.createServer(common.mustCall((req, res) => {
    const chunks = [];
    req.on('data', (chunk) => chunks.push(chunk));
    req.on('end', common.mustCall(() => {
      const body = Buffer.concat(chunks);
      assert.strictEqual(body.toString(), req.url === '/unicode' ? payload.toString() : '');
      res.setHeader('Content-Type', 'text/plain; charset=utf-8');
      res.setHeader('Content-Length', body.byteLength);
      // Split the emoji across writes, so user decoding and inspector byte
      // capture both have to preserve the original UTF-8 sequence.
      res.write(body.subarray(0, 8));
      setImmediate(() => res.end(body.subarray(8)));
    }));
  }, 3));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));

  const cases = [
    {
      path: '/empty-get', method: 'GET', body: '',
      send(req) {
        req.write('');
        req.write(Buffer.alloc(0));
        req.end('');
      },
    },
    {
      path: '/empty-post', method: 'POST', body: '',
      send(req) { req.end(Buffer.alloc(0)); },
    },
    {
      path: '/unicode', method: 'POST', body: payload.toString(),
      send(req) {
        req.write(Buffer.alloc(0));
        req.write(payload.subarray(0, 8));
        req.write('');
        req.end(payload.subarray(8));
      },
    },
  ];

  for (const testCase of cases) {
    const sent = once(session, 'Network.requestWillBeSent');
    const finished = once(session, 'Network.loadingFinished');
    const response = new Promise((resolve) => {
      const req = http.request({
        host: '127.0.0.1', port: server.address().port,
        path: testCase.path, method: testCase.method,
        headers: { 'Content-Type': 'text/plain; charset=utf-8' },
      }, common.mustCall((res) => {
        let body = '';
        res.setEncoding('utf8');
        res.on('data', (chunk) => { body += chunk; });
        res.on('end', common.mustCall(() => {
          assert.strictEqual(body, testCase.body);
          resolve();
        }));
      }));
      req.on('error', common.mustNotCall());
      testCase.send(req);
    });
    const [{ params }] = await sent;
    assert.strictEqual(params.request.hasPostData, testCase.body !== '');
    await Promise.all([response, finished]);
    const { postData } = await session.post('Network.getRequestPostData', { requestId: params.requestId });
    assert.strictEqual(postData, testCase.body);
    const { body, base64Encoded } = await session.post('Network.getResponseBody', { requestId: params.requestId });
    assert.strictEqual(body, testCase.body);
    assert.strictEqual(base64Encoded, false);
  }
  dc.unsubscribe('http.client.request.bodyChunkSent', onChunk);
  dc.unsubscribe('http.client.request.bodySent', onBody);
  session.disconnect();
  await new Promise((resolve) => server.close(resolve));
}
main().then(common.mustCall());
