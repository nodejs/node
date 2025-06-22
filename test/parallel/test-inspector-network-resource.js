// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const { NodeInstance } = require('../common/inspector-helper');
const test = require('node:test');
const assert = require('node:assert');
const path = require('path');
const fs = require('fs');

const resourceUrl = 'http://localhost:3000/app.js';
const resourcePath = path.join(__dirname, '../fixtures/inspector-network-resource/app.js.map');

const script = `
  const { NetworkResources } = require('node:inspector');
  const fs = require('fs');
  NetworkResources.put('${resourceUrl}', fs.readFileSync('${resourcePath.replace(/\\/g, '\\').replace(/'/g, "\\'")}', 'utf8'));
  console.log('Network resource loaded:', '${resourceUrl}');
`;

async function setupSessionAndPauseAtEvalLine4(script) {
  const instance = new NodeInstance([
    '--inspect-brk=0',
    '--experimental-inspector-network-resource',
  ], script);
  const session = await instance.connectInspectorSession();
  await session.send({ method: 'NodeRuntime.enable' });
  await session.waitForNotification('NodeRuntime.waitingForDebugger');
  await session.send({ method: 'Runtime.enable' });
  await session.send({ method: 'Debugger.enable' });
  await session.send({ method: 'Runtime.runIfWaitingForDebugger' });
  await session.waitForNotification((notification) => {
    return (
      notification.method === 'Debugger.scriptParsed' &&
      notification.params.url.includes('[eval]')
    );
  });
  // Set breakpoint at line 4 of [eval] script
  await session.send({
    method: 'Debugger.setBreakpointByUrl',
    params: {
      lineNumber: 4,
      url: '[eval]'
    }
  });
  await session.waitForNotification('Debugger.paused');
  await session.send({ method: 'Debugger.resume' });
  await session.waitForNotification('Debugger.paused');
  return { instance, session };
}

test('should load and stream a static network resource using loadNetworkResource and IO.read', async () => {
  const { session } = await setupSessionAndPauseAtEvalLine4(script);
  const { resource } = await session.send({
    method: 'Network.loadNetworkResource',
    params: { url: resourceUrl },
  });
  assert(resource.success, 'Resource should be loaded successfully');
  assert(resource.stream, 'Resource should have a stream handle');
  let result = await session.send({ method: 'IO.read', params: { handle: resource.stream } });
  let data = result.data;
  let eof = result.eof;
  let content = '';
  while (!eof) {
    content += data;
    result = await session.send({ method: 'IO.read', params: { handle: resource.stream } });
    data = result.data;
    eof = result.eof;
  }
  content += data;
  const expected = fs.readFileSync(resourcePath, 'utf8');
  assert.strictEqual(content, expected);
  await session.send({ method: 'IO.close', params: { handle: resource.stream } });
  await session.send({ method: 'Debugger.resume' });
  await session.waitForDisconnect();
});

test('should return success: false for missing resource', async () => {
  const { session } = await setupSessionAndPauseAtEvalLine4(script);
  const { resource } = await session.send({
    method: 'Network.loadNetworkResource',
    params: { url: 'http://localhost:3000/does-not-exist.js' },
  });
  assert.strictEqual(resource.success, false);
  assert(!resource.stream, 'No stream should be returned for missing resource');
  await session.send({ method: 'Debugger.resume' });
  await session.waitForDisconnect();
});

test('should error or return empty for wrong stream id', async () => {
  const { session } = await setupSessionAndPauseAtEvalLine4(script);
  const { resource } = await session.send({
    method: 'Network.loadNetworkResource',
    params: { url: resourceUrl },
  });
  assert(resource.success);
  const bogus = '999999';
  const result = await session.send({ method: 'IO.read', params: { handle: bogus } });
  assert(result.eof, 'Should be eof for bogus stream id');
  assert.strictEqual(result.data, '');
  await session.send({ method: 'IO.close', params: { handle: resource.stream } });
  await session.send({ method: 'Debugger.resume' });
  await session.waitForDisconnect();
});

test('should support IO.read with size and offset', async () => {
  const { session } = await setupSessionAndPauseAtEvalLine4(script);
  const { resource } = await session.send({
    method: 'Network.loadNetworkResource',
    params: { url: resourceUrl },
  });
  assert(resource.success);
  assert(resource.stream);
  const expected = fs.readFileSync(resourcePath, 'utf8');
  let result = await session.send({ method: 'IO.read', params: { handle: resource.stream, size: 5 } });
  assert.strictEqual(result.data, expected.slice(0, 5));
  result = await session.send({ method: 'IO.read', params: { handle: resource.stream, offset: 5, size: 5 } });
  assert.strictEqual(result.data, expected.slice(5, 10));
  result = await session.send({ method: 'IO.read', params: { handle: resource.stream, offset: 10 } });
  assert.strictEqual(result.data, expected.slice(10));
  await session.send({ method: 'IO.close', params: { handle: resource.stream } });
  await session.send({ method: 'Debugger.resume' });
  await session.waitForDisconnect();
});
