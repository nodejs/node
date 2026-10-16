// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { once } = require('node:events');
const { readFileSync } = require('node:fs');
const http = require('node:http');
const { Session } = require('node:inspector/promises');

async function main() {
  const sessions = [new Session(), new Session()];
  for (const session of sessions) {
    session.connect();
    await session.post('Debugger.enable');
    await session.post('Network.enable');
  }
  const server = http.createServer(common.mustCall((req, res) => res.end()));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const events = sessions.map((session) => once(session, 'Network.requestWillBeSent'));
  const finished = sessions.map((session) => once(session, 'Network.loadingFinished'));

  // Capture the creation site before a later callback writes the body. Native
  // capture must not depend on the user-configurable Error stack formatting.
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
    nested(20);
  }
  originalCaller();
  Error.stackTraceLimit = oldLimit;
  Error.prepareStackTrace = oldPrepare;

  setImmediate(function sendFromLaterCallback() {
    request.end('hello');
  });
  const notifications = await Promise.all(events);
  const initiator = notifications[0][0].params.initiator;
  const frames = initiator.stack.callFrames;
  const frame = frames.find((frame) => frame.functionName === 'createRequest' && frame.url === __filename);
  assert.ok(frame);
  const source = readFileSync(__filename, 'utf8');
  const creationLine = source.split('\n').findIndex((line) => line.trim().startsWith('request = http.request('));
  assert.strictEqual(frame.lineNumber, creationLine);
  assert.ok(frame.columnNumber >= 0);
  assert.ok(frames.some((frame) => frame.functionName === 'originalCaller'));
  assert.ok(!frames.some((frame) => frame.functionName === 'sendFromLaterCallback'));
  for (const frame of frames) {
    assert.match(frame.scriptId, /^\d+$/);
  }

  await Promise.all(finished);
  for (let i = 0; i < sessions.length; i++) {
    const { params } = notifications[i][0];
    // Each session must receive the original snapshot, even after the first
    // session has used it to construct its protocol event.
    assert.deepStrictEqual(params.initiator, initiator);
    const { scriptSource } = await sessions[i].post('Debugger.getScriptSource', { scriptId: frame.scriptId });
    assert.strictEqual(scriptSource, source);
    const { postData } = await sessions[i].post('Network.getRequestPostData', { requestId: params.requestId });
    assert.strictEqual(postData, 'hello');
  }
  for (const session of sessions) session.disconnect();
  await new Promise((resolve) => server.close(resolve));
}
main().then(common.mustCall());
