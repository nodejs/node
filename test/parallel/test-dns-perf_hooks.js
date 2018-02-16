'use strict';

const common = require('../common');
const assert = require('assert');
const dns = require('dns');
const dnstools = require('../common/dns');
const dgram = require('dgram');
const Countdown = require('../common/countdown');

const { PerformanceObserver, performance } = require('perf_hooks');

const obs = new PerformanceObserver(common.mustCall((items) => {
  const entry = items.getEntries()[0];
  assert.strictEqual(entry.name, 'example.org');
  assert.strictEqual(entry.entryType, 'dns');
  assert.strictEqual(typeof entry.startTime, 'number');
  assert.strictEqual(typeof entry.duration, 'number');
  performance.clearDNS();
}, 3));
obs.observe({ entryTypes: ['dns'] });

const answers = [
  { type: 'A', address: '1.2.3.4', ttl: 123 },
  { type: 'AAAA', address: '::42', ttl: 123 }
];

const server = dgram.createSocket('udp4');

const countdown = new Countdown(3, () => server.close());

server.on('message', common.mustCall((msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);
  const domain = parsed.questions[0].domain;

  server.send(dnstools.writeDNSPacket({
    id: parsed.id,
    questions: parsed.questions,
    answers: answers.map((answer) => Object.assign({ domain }, answer)),
  }), port, address);
}, 3));

server.bind(0, common.mustCall(() => {
  const address = server.address();
  dns.setServers([`127.0.0.1:${address.port}`]);

  dns.resolveAny('example.org', common.mustCall(() => countdown.dec()));
  dns.resolve4('example.org', common.mustCall(() => countdown.dec()));
  dns.resolve6('example.org', common.mustCall(() => countdown.dec()));
}));
