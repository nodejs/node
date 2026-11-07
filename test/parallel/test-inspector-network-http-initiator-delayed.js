// Flags: --inspect=0 --experimental-network-inspection
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

  // Capture the original caller even if the body is sent from a later callback,
  // and do not depend on user changes to Error stack formatting or limits.
  const oldLimit = Error.stackTraceLimit;
  const oldPrepare = Error.prepareStackTrace;
  Error.stackTraceLimit = 0;
  Error.prepareStackTrace = common.mustNotCall();
  let request;
  function createRequest() {
    // Keep this next line paired with the marker used below.
    request = http.request({
      host: '127.0.0.1',
      port: server.address().port,
      method: 'POST',
      headers: { 'Content-Type': 'text/plain; charset=utf-8' },
    }, (res) => res.resume());
    request.on('error', common.mustNotCall());
  }

  function nested(depth) {
    if (depth === 0) createRequest();
    else nested(depth - 1);
  }

  function originalCaller() {
    nested(220);
  }
  const source = require('node:fs').readFileSync(__filename, 'utf8').split('\n');
  const creationLine = source.findIndex((line) => line.trim().startsWith('request = http.request('));
  originalCaller();
  Error.stackTraceLimit = oldLimit;
  Error.prepareStackTrace = oldPrepare;

  const event = new Promise((resolve) => session.once('Network.requestWillBeSent', resolve));
  const finished = new Promise((resolve) => session.once('Network.loadingFinished', resolve));
  setImmediate(function sendFromLaterCallback() {
    request.end('hello');
  });
  const { params } = await event;
  const frames = params.initiator.stack.callFrames;
  assert.strictEqual(frames[0].url, __filename);
  assert.strictEqual(frames[0].functionName, 'createRequest');
  assert.strictEqual(frames[0].lineNumber, creationLine);
  assert.ok(frames[0].columnNumber >= 0);
  assert.ok(frames.some((frame) => frame.functionName === 'originalCaller'));
  assert.ok(!frames.some((frame) => frame.functionName === 'sendFromLaterCallback'));
  await finished;
  const { postData } = await session.post('Network.getRequestPostData', { requestId: params.requestId });
  assert.strictEqual(postData, 'hello');
  session.disconnect();
  await new Promise((resolve) => server.close(resolve));
}
main().then(common.mustCall());
