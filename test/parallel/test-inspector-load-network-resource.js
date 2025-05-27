'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const http = require('node:http');
const fs = require('node:fs');
const { NodeInstance } = require('../common/inspector-helper');
const assert = require('node:assert');

common.skipIfInspectorDisabled();

async function test({
  serverCb,
  assertLoadMapResult,
  assertReadMapResult,
  assertLoadTsResult,
  assertReadTsResult,
  readMap,
  getMapUrl,
}) {
  const child = new NodeInstance(
    [
      '--inspect-brk=0',
      '--experimental-inspector-network-resource',
    ],
    '',
    fixtures.path('inspector-network-resource/app.js')
  );

  const server = http.createServer(serverCb);

  server.listen(0);

  const session = await child.connectInspectorSession();

  await session.send({ method: 'NodeRuntime.enable' });
  await session.waitForNotification('NodeRuntime.waitingForDebugger');
  await session.send({ method: 'Runtime.enable' });
  await session.send({ method: 'Debugger.enable' });
  await session.send({ method: 'Runtime.runIfWaitingForDebugger' });
  await session.waitForNotification((notification) => {
    return (
      notification.method === 'Debugger.scriptParsed' &&
      notification.params.url.includes('app.js')
    );
  });

  let result = await session.send({
    method: 'Network.loadNetworkResource',
    params: {
      url: getMapUrl ?
        getMapUrl(server) :
        'http://localhost:' + server.address().port + '/app.js.map',
    },
  });
  let streamId = result.resource.stream || 'notExistsStreamId';

  assertLoadMapResult(result);

  result = await (readMap ?
    readMap(session, streamId) :
    session.send({
      method: 'IO.read',
      params: {
        handle: streamId,
      },
    }));

  assertReadMapResult(result);

  await session.send({
    method: 'IO.close',
    params: {
      handle: streamId,
    },
  });

  result = await session.send({
    method: 'Network.loadNetworkResource',
    params: {
      url: 'http://localhost:' + server.address().port + '/app.ts',
    },
  });

  streamId = result.resource.stream || 'notExistsStreamId';
  assertLoadTsResult(result);
  result = await session.send({
    method: 'IO.read',
    params: {
      handle: streamId,
    },
  });
  assertReadTsResult(result);
  await session.send({
    method: 'IO.close',
    params: {
      handle: streamId,
    },
  });

  server.close();
  await session.waitForNotification('Debugger.paused');
  await session.send({ method: 'Debugger.resume' });
  await session.waitForDisconnect();
}

const mapText = fs.readFileSync(
  fixtures.path('inspector-network-resource/app.js.map'),
  'utf8'
);

const main = {
  serverCb: (req, res) => {
    const filename = req.url.split('/').pop();
    const data = fs.readFileSync(
      fixtures.path('inspector-network-resource/' + filename)
    );
    res.writeHead(200, {
      'Content-Type': 'application/javascript',
      'Content-Length': data.length,
    });
    res.end(data);
  },
  assertLoadMapResult: (result) => {
    assert.ok(result.resource.success);
    assert.ok(result.resource.stream);
  },
  assertReadMapResult: (result) => {
    assert.ok(result.data);
    assert.ok(result.data.length > 0);
    assert.ok(result.data.includes(mapText));
  },
  assertLoadTsResult: (result) => {
    assert.ok(result.resource.success);
    assert.ok(result.resource.stream);
  },
  assertReadTsResult: (result) => {
    assert.ok(result.data);
    assert.ok(result.data.length > 0);
    assert.ok(
      result.data.includes('function add(a: number, b: number): number {')
    );
  },
};

const timeout = {
  serverCb: (req, res) => {},
  assertLoadMapResult: (result) => {
    assert.strictEqual(result.resource.success, false);
  },
  assertReadMapResult: (result) => {
    assert.strictEqual(result.data, '');
  },
  assertLoadTsResult: (result) => {
    assert.strictEqual(result.resource.success, false);
  },
  assertReadTsResult: (result) => {
    assert.strictEqual(result.data, '');
  },
};

const smallChunkRead = {
  ...main,
  readMap: async (session, stream) => {
    let result = await session.send({
      method: 'IO.read',
      params: { handle: stream, size: 10 },
    });
    assert.strictEqual(result.data.length, 10);
    assert.strictEqual(result.data, mapText.slice(0, 10));
    result = await session.send({
      method: 'IO.read',
      params: {
        handle: stream,
        size: 10,
        offset: 10,
      },
    });
    assert.strictEqual(result.data.length, 10);
    assert.strictEqual(result.data, mapText.slice(10, 20));

    result = await session.send({
      method: 'IO.read',
      params: {
        handle: stream,
        size: 10,
        offset: 30,
      },
    });

    assert.strictEqual(result.data.length, 10);
    assert.strictEqual(result.data, mapText.slice(30, 40));

    result = await session.send({
      method: 'IO.read',
      params: {
        handle: stream,
        size: 10,
        offset: mapText.length - 10,
      },
    });

    assert.strictEqual(result.data.length, 10);
    assert.strictEqual(result.data, mapText.slice(mapText.length - 10));

    result = await session.send({
      method: 'IO.read',
      params: {
        handle: stream,
        size: 10,
      },
    });

    assert.strictEqual(result.data.length, 0);
    assert.strictEqual(result.eof, true);

    return result;
  },
  assertReadMapResult: () => {},
};

const invalidUrl = {
  ...timeout,
  getMapUrl: () => 'invalid-url',
};

test(main).then(common.mustCall());
test(timeout).then(common.mustCall());
test(smallChunkRead).then(common.mustCall());
test(invalidUrl).then(common.mustCall());
