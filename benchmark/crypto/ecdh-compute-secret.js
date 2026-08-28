'use strict';

const common = require('../common.js');
const assert = require('node:assert');
const crypto = require('node:crypto');

const kCurve = 'prime256v1';
const kPeerPoolSize = 32;
const scenarios = [
  'first-after-generate',
  'full-lifecycle',
  'reused-local-same-peer',
  'reused-local-peer-pool',
];

const bench = common.createBenchmark(main, {
  scenario: scenarios,
  n: [5_000],
}, {
  test: { scenario: scenarios, n: 1 },
});

function generateContext() {
  const context = crypto.createECDH(kCurve);
  context.generateKeys();
  return context;
}

function verifySecret(secret, local, peer) {
  assert.deepStrictEqual(secret, peer.computeSecret(local.getPublicKey()));
}

function firstAfterGenerate(n) {
  const peer = generateContext();
  const peerPublicKey = peer.getPublicKey();
  const warmup = generateContext();
  warmup.computeSecret(peerPublicKey);

  const locals = Array.from({ length: n }, generateContext);
  const secrets = new Array(n);

  bench.start();
  for (let i = 0; i < n; i++)
    secrets[i] = locals[i].computeSecret(peerPublicKey);
  bench.end(n);

  verifySecret(secrets[n - 1], locals[n - 1], peer);
}

function fullLifecycle(n) {
  const peer = generateContext();
  const peerPublicKey = peer.getPublicKey();
  const warmup = generateContext();
  warmup.computeSecret(peerPublicKey);

  const locals = new Array(n);
  const secrets = new Array(n);

  bench.start();
  for (let i = 0; i < n; i++) {
    const local = locals[i] = generateContext();
    secrets[i] = local.computeSecret(peerPublicKey);
  }
  bench.end(n);

  verifySecret(secrets[n - 1], locals[n - 1], peer);
}

function reusedLocalSamePeer(n) {
  const local = generateContext();
  const peer = generateContext();
  const peerPublicKey = peer.getPublicKey();
  local.computeSecret(peerPublicKey);

  const secrets = new Array(n);

  bench.start();
  for (let i = 0; i < n; i++)
    secrets[i] = local.computeSecret(peerPublicKey);
  bench.end(n);

  verifySecret(secrets[n - 1], local, peer);
}

function reusedLocalPeerPool(n) {
  const local = generateContext();
  const peers = Array.from(
    { length: Math.min(n, kPeerPoolSize) },
    generateContext);
  const peerPublicKeys = peers.map((peer) => peer.getPublicKey());
  local.computeSecret(peerPublicKeys[0]);

  const secrets = new Array(n);

  bench.start();
  for (let i = 0; i < n; i++)
    secrets[i] = local.computeSecret(peerPublicKeys[i % peers.length]);
  bench.end(n);

  const lastPeer = peers[(n - 1) % peers.length];
  verifySecret(secrets[n - 1], local, lastPeer);
}

function main({ scenario, n }) {
  switch (scenario) {
    case 'first-after-generate':
      return firstAfterGenerate(n);
    case 'full-lifecycle':
      return fullLifecycle(n);
    case 'reused-local-same-peer':
      return reusedLocalSamePeer(n);
    case 'reused-local-peer-pool':
      return reusedLocalPeerPool(n);
    default:
      throw new Error(`Unsupported scenario: ${scenario}`);
  }
}
