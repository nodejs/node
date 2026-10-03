// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const http = require('node:http');
const { Session } = require('node:inspector/promises');

// The inspector cache must own its bytes even when the HTTP caller reuses
// a Buffer or a view into a larger Uint8Array after writing it.
async function main() {
  const session = new Session();
  session.connect();
  await session.post('Network.enable');

  const server = http.createServer(common.mustCall((req, res) => {
    const chunks = [];
    req.on('data', (chunk) => chunks.push(chunk));
    req.on('end', common.mustCall(() => {
      assert.strictEqual(Buffer.concat(chunks).toString(), 'buffer-view-ü');
      res.end();
    }));
  }));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));

  let requestId;
  session.on('Network.requestWillBeSent', common.mustCall(({ params }) => {
    requestId = params.requestId;
    assert.strictEqual(params.request.hasPostData, true);
  }));
  const finished = new Promise((resolve) => {
    session.once('Network.loadingFinished', resolve);
  });
  const req = http.request({
    host: '127.0.0.1',
    port: server.address().port,
    method: 'POST',
    headers: { 'Content-Type': 'text/plain; charset=utf-8' },
  }, common.mustCall((res) => res.resume()));
  req.on('error', common.mustNotCall());

  const buffer = Buffer.from('buffer-');
  await new Promise((resolve) => req.write(buffer, resolve));
  buffer.fill(0x78);

  const backing = new Uint8Array(Buffer.from('!view-?'));
  const view = backing.subarray(1, backing.length - 1);
  await new Promise((resolve) => req.write(view, resolve));
  backing.fill(0x79);
  req.end('ü', 'utf8');

  await finished;
  const { postData } = await session.post('Network.getRequestPostData', { requestId });
  assert.strictEqual(postData, 'buffer-view-ü');
  session.disconnect();
  await new Promise((resolve) => server.close(resolve));
}

main().then(common.mustCall());
