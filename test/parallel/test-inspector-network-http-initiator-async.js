// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('node:assert');
const { once } = require('node:events');
const http = require('node:http');
const { Session } = require('node:inspector/promises');

async function main() {
  const session = new Session();
  session.connect();
  await session.post('Network.enable');
  const server = http.createServer(common.mustCall((req, res) => res.end()));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const event = once(session, 'Network.requestWillBeSent');
  const finished = once(session, 'Network.loadingFinished');
  async function requestFromAsync() {
    await Promise.resolve();
    const req = http.request({ port: server.address().port }, (res) => res.resume());
    req.on('error', common.mustNotCall());
    req.end();
  }

  await requestFromAsync();
  const [{ params }] = await event;
  const frame = params.initiator.stack.callFrames.find((frame) => frame.functionName === 'requestFromAsync');
  assert.ok(frame);
  assert.strictEqual(frame.url, __filename);
  assert.match(frame.scriptId, /^\d+$/);
  await finished;
  session.disconnect();
  await new Promise((resolve) => server.close(resolve));
}
main().then(common.mustCall());
