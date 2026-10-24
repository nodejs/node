// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

// Network tracking is subscribed at startup. Requests that start before
// Network.enable must not produce events after Network.enable, since the
// frontend never saw their Network.requestWillBeSent.

const assert = require('node:assert');
const { once } = require('node:events');
const http = require('node:http');
const inspector = require('node:inspector/promises');

const session = new inspector.Session();
session.connect();

const events = [];
for (const method of [
  'requestWillBeSent',
  'responseReceived',
  'loadingFinished',
  'loadingFailed',
]) {
  session.on(`Network.${method}`, ({ params }) => {
    events.push({ method, requestId: params.requestId, url: params.request?.url });
  });
}

const held = new Map();
const server = http.createServer((req, res) => {
  if (req.url === '/after') {
    res.end('after');
  } else {
    held.set(req.url, res);
  }
});

async function waitForHeld(url) {
  while (!held.has(url)) {
    await new Promise((resolve) => setImmediate(resolve));
  }
  return held.get(url);
}

async function main() {
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const base = `http://127.0.0.1:${server.address().port}`;

  const httpResponse = new Promise((resolve) => {
    http.get(`${base}/http`, (res) => {
      res.resume();
      res.on('end', resolve);
    });
  });
  const httpError = new Promise((resolve) => {
    http.get(`${base}/http-error`).on('error', resolve);
  });
  const fetchResponse = fetch(`${base}/fetch`).then((res) => res.text());

  const resHttp = await waitForHeld('/http');
  const resHttpError = await waitForHeld('/http-error');
  const resFetch = await waitForHeld('/fetch');

  await session.post('Network.enable');

  resHttp.end('http');
  resHttpError.socket.destroy();
  resFetch.end('fetch');
  await Promise.all([httpResponse, httpError, fetchResponse]);

  assert.deepStrictEqual(events, []);

  await new Promise((resolve) => {
    http.get(`${base}/after`, (res) => {
      res.resume();
      res.on('end', resolve);
    });
  });
  await new Promise((resolve) => setImmediate(resolve));

  assert.deepStrictEqual(events.map((e) => e.method), [
    'requestWillBeSent',
    'responseReceived',
    'loadingFinished',
  ]);
  assert.strictEqual(events[0].url, `${base}/after`);
  assert(events.every((e) => e.requestId === events[0].requestId));

  session.disconnect();
  server.close();
}

main().then(common.mustCall());
