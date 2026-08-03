'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const mutable = Buffer.from('A');
let mutableResponses = 0;

function sequence(length) {
  return Uint8Array.from({ length }, (_, index) => index & 0xff);
}

const bodies = {
  '/buffer-1024': Buffer.from(sequence(1024)),
  '/buffer-1025': Buffer.from(sequence(1025)),
  '/uint8array-1024': sequence(1024),
};

const server = http.createServer(common.mustCall((req, res) => {
  let body;
  if (req.url === '/mutable') {
    mutable[0] = 0x41 + mutableResponses++;
    body = mutable;
  } else {
    body = bodies[req.url];
    assert(body);
  }

  res.writeHead(200, {
    'Content-Type': 'application/octet-stream',
    'Content-Length': String(body.byteLength),
  });
  res.end(body);
}, 5));

async function main() {
  await new Promise((resolve) => {
    server.listen(0, common.localhostIPv4, common.mustCall(resolve));
  });

  const agent = new http.Agent({ keepAlive: true, maxSockets: 1 });
  async function request(path) {
    return new Promise((resolve, reject) => {
      const req = http.get({
        agent,
        host: common.localhostIPv4,
        path,
        port: server.address().port,
      }, common.mustCall((res) => {
        const chunks = [];
        res.on('data', (chunk) => chunks.push(chunk));
        res.on('end', common.mustCall(() => resolve(Buffer.concat(chunks))));
      }));
      req.on('error', reject);
    });
  }

  assert.deepStrictEqual(await request('/mutable'), Buffer.from('A'));
  assert.deepStrictEqual(await request('/mutable'), Buffer.from('B'));
  assert.deepStrictEqual(await request('/buffer-1024'), bodies['/buffer-1024']);
  assert.deepStrictEqual(await request('/buffer-1025'), bodies['/buffer-1025']);
  assert.deepStrictEqual(
    await request('/uint8array-1024'),
    Buffer.from(bodies['/uint8array-1024']),
  );

  agent.destroy();
  await new Promise((resolve) => server.close(common.mustCall(resolve)));
}

main().then(common.mustCall());
