// Flags: --experimental-network-inspection
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('node:assert');
const http = require('node:http');
const { Session } = require('node:inspector/promises');

async function main() {
  const session = new Session();
  session.connect();
  await session.post('Network.enable');
  const server = http.createServer(common.mustCall((req, res) => res.end()));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const event = new Promise((resolve) => session.once('Network.requestWillBeSent', resolve));
  const finished = new Promise((resolve) => session.once('Network.loadingFinished', resolve));
  async function requestFromAsync() {
    await Promise.resolve();
    const req = http.request({ port: server.address().port }, (res) => res.resume());
    req.on('error', common.mustNotCall());
    req.end();
  }

  async function parentAsync() {
    await requestFromAsync();
  }
  await parentAsync();
  const { params } = await event;
  const frames = params.initiator.stack.callFrames;
  assert.strictEqual(frames[0].functionName, 'requestFromAsync');
  assert.strictEqual(frames[0].url, __filename);
  assert.ok(frames.some((frame) => frame.functionName === 'parentAsync' && frame.url === __filename));
  assert.ok(frames.some((frame) => frame.functionName === 'main' && frame.url === __filename));
  await finished;
  session.disconnect();
  await new Promise((resolve) => server.close(resolve));
}
main().then(common.mustCall());
