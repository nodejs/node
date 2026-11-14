'use strict';
// Tests dns.resolveSvcb() and dns.resolveHttps() (RFC 9460) against a local
// DNS server.

const common = require('../common');
const dnstools = require('../common/dns');
const dns = require('dns');
const assert = require('assert');
const dgram = require('dgram');
const dnsPromises = dns.promises;

const u16 = (n) => Buffer.from([n >> 8, n & 0xff]);
const alpn = (...ids) =>
  Buffer.concat(ids.map((id) => Buffer.from([id.length, ...Buffer.from(id)])));
const ech = Buffer.from('0045fe0d0041', 'hex');

const zone = {
  // ServiceMode SVCB record carrying private-use keys, as used by DNS-AID.
  '_index._mcp._agents.example.org': [{
    type: 'SVCB',
    priority: 1,
    target: 'mcp.example.org',
    params: [
      [1, alpn('mcp')],
      [3, u16(443)],
      [65400, Buffer.from('https://aid.example.org/cap')],
      [65404, Buffer.from('tenant-42')],
    ],
  }],
  'example.org': [{
    type: 'HTTPS',
    priority: 1,
    target: '.',
    params: [
      [0, Buffer.concat([u16(1), u16(3)])],
      [1, alpn('h2', 'h3')],
      [2, Buffer.alloc(0)],
      [3, u16(8443)],
      [4, Buffer.from([192, 0, 2, 1, 192, 0, 2, 2])],
      [5, ech],
      [6, Buffer.from('20010db8000000000000000000000001', 'hex')],
      [7, Buffer.from('/dns-query{?dns}')],
      [8, Buffer.alloc(0)],
    ],
  }],
  'alias.example.org': [{
    type: 'HTTPS',
    priority: 0,
    target: 'svc.example.net',
  }],
  'bad-alpn.example.org': [{
    type: 'SVCB',
    priority: 1,
    target: '.',
    params: [[1, Buffer.from([5, 0x68, 0x32])]],
  }],
  'bad-order.example.org': [{
    type: 'SVCB',
    priority: 1,
    target: '.',
    params: [[3, u16(443)], [1, alpn('h2')]],
  }],
  'bad-port.example.org': [{
    type: 'SVCB',
    priority: 1,
    target: '.',
    params: [[3, Buffer.from([1, 2, 3])]],
  }],
  // "mandatory" must not list itself.
  'bad-mandatory-self.example.org': [{
    type: 'SVCB',
    priority: 1,
    target: '.',
    params: [[0, Buffer.concat([u16(0), u16(1)])], [1, alpn('h2')]],
  }],
  // Keys listed in "mandatory" must be in increasing order.
  'bad-mandatory-order.example.org': [{
    type: 'SVCB',
    priority: 1,
    target: '.',
    params: [
      [0, Buffer.concat([u16(3), u16(1)])],
      [1, alpn('h2')],
      [3, u16(443)],
    ],
  }],
  // Keys listed in "mandatory" must be present.
  'bad-mandatory-missing.example.org': [{
    type: 'SVCB',
    priority: 1,
    target: '.',
    params: [[0, u16(3)], [1, alpn('h2')]],
  }],
  // SvcParams of AliasMode records are ignored, even if malformed.
  'alias-params.example.org': [{
    type: 'SVCB',
    priority: 0,
    target: 'svc.example.net',
    params: [[3, Buffer.from([1])]],
  }],
  // A malformed HTTPS RRset must not hide other records from resolveAny().
  'any-mixed.example.org': [{
    type: 'A',
    address: '192.0.2.1',
  }, {
    type: 'HTTPS',
    priority: 1,
    target: '.',
    params: [[3, Buffer.from([1])]],
  }],
};

const server = dgram.createSocket('udp4');

server.on('message', common.mustCallAtLeast((msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);
  const { domain, type } = parsed.questions[0];
  const answers = (zone[domain] ?? [])
    .filter((rr) => type === 'ANY' || rr.type === type)
    .map((rr) => ({ domain, ttl: 300, ...rr }));

  server.send(dnstools.writeDNSPacket({
    id: parsed.id,
    questions: parsed.questions,
    answers,
  }), port, address);
}));

function assertArrayBuffer(actual, expected) {
  assert.ok(actual instanceof ArrayBuffer);
  assert.deepStrictEqual(Buffer.from(actual), expected);
}

function validateDnsAid(records) {
  assert.strictEqual(records.length, 1);
  const [record] = records;
  assert.strictEqual(record.priority, 1);
  assert.strictEqual(record.target, 'mcp.example.org');
  assert.strictEqual(Object.hasOwn(record, 'type'), false);
  assert.deepStrictEqual(Object.keys(record.params),
                         ['alpn', 'port', 'key65400', 'key65404']);
  assert.deepStrictEqual(record.params.alpn, ['mcp']);
  assert.strictEqual(record.params.port, 443);
  assertArrayBuffer(record.params.key65400,
                    Buffer.from('https://aid.example.org/cap'));
  assertArrayBuffer(record.params.key65404, Buffer.from('tenant-42'));
}

function validateHttps(records) {
  assert.strictEqual(records.length, 1);
  const [{ priority, target, params }] = records;
  assert.strictEqual(priority, 1);
  assert.strictEqual(target, '');
  assert.deepStrictEqual(params.mandatory, ['alpn', 'port']);
  assert.deepStrictEqual(params.alpn, ['h2', 'h3']);
  assert.strictEqual(params['no-default-alpn'], true);
  assert.strictEqual(params.port, 8443);
  assert.deepStrictEqual(params.ipv4hint, ['192.0.2.1', '192.0.2.2']);
  assertArrayBuffer(params.ech, ech);
  assert.deepStrictEqual(params.ipv6hint, ['2001:db8::1']);
  assert.strictEqual(params.dohpath, '/dns-query{?dns}');
  assert.strictEqual(params.ohttp, true);
}

server.bind(0, common.mustCall(async () => {
  const servers = [`127.0.0.1:${server.address().port}`];
  const resolver = new dnsPromises.Resolver();
  resolver.setServers(servers);
  dns.setServers(servers);

  validateDnsAid(await resolver.resolveSvcb('_index._mcp._agents.example.org'));
  validateDnsAid(
    await resolver.resolve('_index._mcp._agents.example.org', 'SVCB'));
  validateHttps(await resolver.resolveHttps('example.org'));
  validateHttps(await resolver.resolve('example.org', 'HTTPS'));

  assert.deepStrictEqual(await resolver.resolveHttps('alias.example.org'), [{
    priority: 0,
    target: 'svc.example.net',
    params: {},
  }]);

  // Only resolveAny() reports the record type.
  const any = await resolver.resolveAny('example.org');
  assert.strictEqual(any.length, 1);
  assert.strictEqual(any[0].type, 'HTTPS');
  validateHttps(any);

  assert.deepStrictEqual(await resolver.resolveSvcb('alias-params.example.org'),
                         [{ priority: 0, target: 'svc.example.net', params: {} }]);

  const anyMixed = await resolver.resolveAny('any-mixed.example.org');
  assert.deepStrictEqual(anyMixed.map(({ type }) => type), ['A']);

  for (const host of ['bad-alpn', 'bad-order', 'bad-port',
                      'bad-mandatory-self', 'bad-mandatory-order',
                      'bad-mandatory-missing']) {
    await assert.rejects(resolver.resolveSvcb(`${host}.example.org`), {
      code: 'EBADRESP',
      syscall: 'querySvcb',
    });
  }

  await assert.rejects(resolver.resolveSvcb('alias.example.org'), {
    code: 'ENODATA',
  });

  dns.resolveSvcb('_index._mcp._agents.example.org', common.mustSucceed(
    (records) => {
      validateDnsAid(records);
      dns.resolveHttps('example.org', common.mustSucceed((records) => {
        validateHttps(records);
        server.close();
      }));
    }));
}));
