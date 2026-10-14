import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'node:events';
import http from 'node:http';
import net from 'node:net';

const CRLF = '\r\n';
const firstRequest = `GET /first HTTP/1.1${CRLF}Host: localhost${CRLF}${CRLF}`;

function switchRequest(eventName) {
  if (eventName === 'connect') {
    return `CONNECT localhost:443 HTTP/1.1${CRLF}` +
           `Host: localhost:443${CRLF}${CRLF}`;
  }

  return `GET /second HTTP/1.1${CRLF}` +
         `Host: localhost${CRLF}` +
         `Connection: Upgrade${CRLF}` +
         `Upgrade: websocket${CRLF}${CRLF}`;
}

function switchResponse(eventName) {
  if (eventName === 'connect') {
    return `HTTP/1.1 200 Connection Established${CRLF}${CRLF}SWITCHED`;
  }

  return `HTTP/1.1 101 Switching Protocols${CRLF}` +
         `Connection: Upgrade${CRLF}` +
         `Upgrade: websocket${CRLF}${CRLF}SWITCHED`;
}

async function listen(server) {
  server.listen(0);
  await once(server, 'listening');
}

async function close(server) {
  await new Promise((resolve) => server.close(resolve));
}

async function testPipelinedSwitchIsRejected(eventName) {
  const server = http.createServer(common.mustCall((req) => {
    assert.strictEqual(req.url, '/first');
  }));

  server.on(eventName, common.mustNotCall());
  await listen(server);

  const client = net.connect(server.address().port);
  let received = '';
  client.setEncoding('latin1');
  client.on('data', (data) => received += data);
  client.on('error', () => {});
  const closed = new Promise((resolve) => client.once('close', resolve));
  client.end(firstRequest + switchRequest(eventName));

  await closed;
  assert.strictEqual(received, '');
  await close(server);
}

async function testSwitchAfterResponseIsAccepted(eventName) {
  const responseFinished = Promise.withResolvers();
  const server = http.createServer(common.mustCall((req, res) => {
    assert.strictEqual(req.url, '/first');
    res.on('finish', responseFinished.resolve);
    res.end('FIRST');
  }));

  server.on(eventName, common.mustCall((req, socket) => {
    socket.end(switchResponse(eventName));
  }));
  await listen(server);

  const client = net.connect(server.address().port);
  let received = '';
  client.setEncoding('latin1');
  client.on('data', (data) => received += data);
  client.on('error', () => {});
  client.write(firstRequest);

  await responseFinished.promise;
  client.end(switchRequest(eventName));
  await once(client, 'close');

  const firstIndex = received.indexOf('FIRST');
  const switchedIndex = received.indexOf('SWITCHED');
  assert.notStrictEqual(firstIndex, -1);
  assert.notStrictEqual(switchedIndex, -1);
  assert.ok(firstIndex < switchedIndex);
  await close(server);
}

for (const eventName of ['upgrade', 'connect']) {
  await testPipelinedSwitchIsRejected(eventName);
  await testSwitchAfterResponseIsAccepted(eventName);
}
