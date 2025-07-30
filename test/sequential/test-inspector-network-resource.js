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

const resourceText = fs.readFileSync(resourcePath, 'utf8');
const embedPath = resourcePath.replace(/\\/g, '\\\\').replace(/'/g, "\\'");
const script = `
  const { NetworkResources } = require('node:inspector');
  const fs = require('fs');
  NetworkResources.put('${resourceUrl}', fs.readFileSync('${embedPath}', 'utf8'));
  console.log('Network resource loaded:', '${resourceUrl}');
  debugger;
`;

async function setupSessionAndPauseAtEvalLastLine(script) {
  const instance = new NodeInstance([
    '--inspect-wait=0',
    '--experimental-inspector-network-resource',
  ], script);
  const session = await instance.connectInspectorSession();
  await session.send({ method: 'NodeRuntime.enable' });
  await session.waitForNotification('NodeRuntime.waitingForDebugger');
  await session.send({ method: 'Runtime.enable' });
  await session.send({ method: 'Debugger.enable' });
  await session.send({ method: 'Runtime.runIfWaitingForDebugger' });
  await session.waitForNotification('Debugger.paused');
  return { instance, session };
}

test('should load and stream a static network resource using loadNetworkResource and IO.read', async () => {
  const { session } = await setupSessionAndPauseAtEvalLastLine(script);
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
  assert.strictEqual(content, resourceText);
  await session.send({ method: 'IO.close', params: { handle: resource.stream } });
  await session.send({ method: 'Debugger.resume' });
  await session.waitForDisconnect();
});

test('should return success: false for missing resource', async () => {
  const { session } = await setupSessionAndPauseAtEvalLastLine(script);
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
  const { session } = await setupSessionAndPauseAtEvalLastLine(script);
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
  const { session } = await setupSessionAndPauseAtEvalLastLine(script);
  const { resource } = await session.send({
    method: 'Network.loadNetworkResource',
    params: { url: resourceUrl },
  });
  assert(resource.success);
  assert(resource.stream);
  let result = await session.send({ method: 'IO.read', params: { handle: resource.stream, size: 5 } });
  assert.strictEqual(result.data, resourceText.slice(0, 5));
  result = await session.send({ method: 'IO.read', params: { handle: resource.stream, offset: 5, size: 5 } });
  assert.strictEqual(result.data, resourceText.slice(5, 10));
  result = await session.send({ method: 'IO.read', params: { handle: resource.stream, offset: 10 } });
  assert.strictEqual(result.data, resourceText.slice(10));
  await session.send({ method: 'IO.close', params: { handle: resource.stream } });
  await session.send({ method: 'Debugger.resume' });
  await session.waitForDisconnect();
});

test('should load resource put from another thread', async () => {
  const workerScript = `
    console.log('this is worker thread');
    debugger;
  `;
  const script = `
  const { NetworkResources } = require('node:inspector');
  const fs = require('fs');
  NetworkResources.put('${resourceUrl}', fs.readFileSync('${embedPath}', 'utf8'));
  const { Worker } = require('worker_threads');
  const worker = new Worker(\`${workerScript}\`, {eval: true});
  `;
  const instance = new NodeInstance([
    '--experimental-inspector-network-resource',
    '--experimental-worker-inspection',
    '--inspect-brk=0',
  ], script);
  const session = await instance.connectInspectorSession();
  await setupInspector(session);
  await session.waitForNotification('Debugger.paused');
  await session.send({ method: 'Debugger.resume' });
  await session.waitForNotification('Target.targetCreated');
  await session.send({ method: 'Target.setAutoAttach', params: { autoAttach: true, waitForDebuggerOnStart: true } });
  let sessionId;
  await session.waitForNotification((notification) => {
    if (notification.method === 'Target.attachedToTarget') {
      sessionId = notification.params.sessionId;
      return true;
    }
    return false;

  });
  await setupInspector(session, sessionId);

  await session.waitForNotification('Debugger.paused');

  const { resource } = await session.send({
    method: 'Network.loadNetworkResource',
    params: { url: resourceUrl, sessionId },
  });

  assert(resource.success, 'Resource should be loaded successfully');
  assert(resource.stream, 'Resource should have a stream handle');
  let result = await session.send({ method: 'IO.read', params: { handle: resource.stream, sessionId } });
  let data = result.data;
  let eof = result.eof;
  let content = '';
  while (!eof) {
    content += data;
    result = await session.send({ method: 'IO.read', params: { handle: resource.stream, sessionId } });
    data = result.data;
    eof = result.eof;
  }
  content += data;
  assert.strictEqual(content, resourceText);
  await session.send({ method: 'IO.close', params: { handle: resource.stream, sessionId } });

  await session.send({ method: 'Debugger.resume', sessionId });

  await session.waitForDisconnect();

  async function setupInspector(session, sessionId) {
    await session.send({ method: 'NodeRuntime.enable', sessionId });
    await session.waitForNotification('NodeRuntime.waitingForDebugger');
    await session.send({ method: 'Runtime.enable', sessionId });
    await session.send({ method: 'Debugger.enable', sessionId });
    await session.send({ method: 'Runtime.runIfWaitingForDebugger', sessionId });
  }
});
